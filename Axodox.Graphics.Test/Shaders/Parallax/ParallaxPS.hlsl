#include "../common.hlsli"
Texture2D<float4> _texture : register(t9);
SamplerState _sampler : register(s0);


cbuffer CameraBuffer : register(b0)
{
    cameraConstants camConstants;
}


cbuffer DebugBuffer : register(b9)
{
    DebugValues debugValues;
}

cbuffer ModelBuffer : register(b1)
{
    float3 center;
    float2 scaling;
};


struct output_t
{
    float4 albedo;
    float4 normal;
    float4 materialValues;
};



cbuffer PSProperties : register(b2)
{
    float3 Albedo;
    float Roughness;
    float foamDepthFalloff;
    float foamRoughnessModifier;
    float NormalDepthAttenuation;
    float _HeightModifier;
    float _WavePeakScatterStrength;
    float _ScatterShadowStrength;
    float _Fresnel;
};







output_t calculate(float4 grad, float4 Screen, float3 localPos, float2 planeCoord)
{

    output_t output;
    
	
    if (has_flag(debugValues.flags, 6))
    {
        float2 texCoord = GetTextureCoordFromPlaneCoordAndPatch(planeCoord, debugValues.patchSizes.r);
        float4 text = _texture.Sample(_sampler, texCoord) * float4(debugValues.pixelMult.
        xyz, 1);
      
        output.albedo = text;
        return output;
    }
        
    float3 normal = normalize(grad.xyz);
    
    const float3 viewVec = camConstants.cameraPos - localPos;
    float3 viewDir = normalize(viewVec);

    if (has_flag(debugValues.flags, 24))
    {
        output.albedo =
         float4(0, 1, 0, 1);
        if (dot(normal, viewDir) < 0)
            output.albedo = float4(1, 0, 0, 1);
        
        return output;
    }

    //if (dot(normal, viewDir) < 0)
    //{
    //    normal *= -1;
    //}

    float Jacobian = grad.w;
    if (has_flag(debugValues.flags, 25))
    {
        output.albedo =
         float4(Jacobian, Jacobian, Jacobian, 1);
        return output;
    }
    
    float depth = Screen.z / Screen.w;

				

				
    float foam = 0;
    if (has_flag(debugValues.flags, 2))
    {
        foam =
    lerp(0.0f, Jacobian, pow(depth, foamDepthFalloff));
    }

    normal = lerp(normal, float3(0, 1, 0), pow(depth, NormalDepthAttenuation));



				
    // Make foam appear rougher
    float a = Roughness + foam * foamRoughnessModifier;

   
    				
    float3 albedo = Albedo;

    //low
    output.albedo = float4(albedo, saturate(foam));
    //high
    output.normal = float4(OctahedronNormalEncode(normal), _Fresnel, 1);
    //low
    output.materialValues = float4(a, _HeightModifier * _WavePeakScatterStrength, _ScatterShadowStrength, 1);
    return output;
}


Texture2D<float2> _coneMap1 : register(t0);
Texture2D<float2> _coneMap2 : register(t1);
Texture2D<float2> _coneMap3 : register(t2);
Texture2D<float4> gradients1 : register(t3);
Texture2D<float4> gradients2 : register(t4);
Texture2D<float4> gradients3 : register(t5);

struct input_t
{
    float4 Position : SV_POSITION;
    float2 planeCoord : PLANECOORD;
};


struct ConeMapData
{
    float height;
    float slope;
};




ConeMapData readConeMap(
    float2 uv
)
{
    ConeMapData r;
    r.height = 0;
    r.slope = 999999;
    if (has_flag(debugValues.flags, 3))
    {
        
        float2 t = _coneMap1.SampleLevel(_sampler, GetTextureCoordFromPlaneCoordAndPatch(uv, debugValues.patchSizes.r), 0);
        r.height += t.x;
        r.slope = min(r.slope, t.y);
    }
    if (has_flag(debugValues.flags, 4))
    {
        float2 t = _coneMap2.SampleLevel(_sampler, GetTextureCoordFromPlaneCoordAndPatch(uv, debugValues.patchSizes.g), 0);
        r.height += t.x;
        r.slope = min(r.slope, t.y);
    }
    if (has_flag(debugValues.flags, 5))
    {
        float2 t = _coneMap3.SampleLevel(_sampler, GetTextureCoordFromPlaneCoordAndPatch(uv, debugValues.patchSizes.b), 0);
        r.height += t.x;
        r.slope = min(r.slope, t.y);
    }


    return r;
}

float4 readGrad(float2 uv)
{
    float4 t1 = gradients1.SampleLevel(_sampler, GetTextureCoordFromPlaneCoordAndPatch(uv, debugValues.patchSizes.r), 0);
    float4 t2 = gradients2.SampleLevel(_sampler, GetTextureCoordFromPlaneCoordAndPatch(uv, debugValues.patchSizes.g), 0);
    float4 t3 = gradients3.SampleLevel(_sampler, GetTextureCoordFromPlaneCoordAndPatch(uv, debugValues.patchSizes.b), 0);

    return t1 + t2 + t3;
}


struct HMapIntersection
{
    float2 uv;
    float t; // uv = (1-t)*u + t*u2
    float last_t;
    bool wasHit;
};

static const HMapIntersection INIT_INTERSECTION = { 0.0.xx, 0.0, 0.0, false };

HMapIntersection findIntersection_bumpMapping(float2 u, float2 u2)
{
    HMapIntersection ret = INIT_INTERSECTION;
    ret.uv = u2;
    ret.t = 1;
    ret.last_t = 1;
    return ret;
}

HMapIntersection findIntersection_parallaxMapping(float2 u, float2 u2)
{
    float t = 1 - getH(u2);
    HMapIntersection ret = INIT_INTERSECTION;
    ret.uv = (1 - t) * u + t * u2;
    ret.t = t;
    ret.last_t = t;
    return ret;
}

// Adapted from "Cone Step Mapping: An Iterative Ray-Heightfield Intersection Algorithm" - Jonathan Dummer
HMapIntersection findIntersection_coneStepMapping(float2 u, float2 u2)
{
    float3 ds = float3(u2 - u, 1);
    ds = normalize(ds);
    float w = 1 / HMres.x;
    float iz = sqrt(1.0 - ds.z * ds.z); // = length(ds.xy)
    float sc = 0;
    ConeMapData tmp = readConeMap(u);
    float2 t = float2(tmp.height, tmp.slope);
    int stepCount = 0;
    float zTimesSc = 0.0;

    float2 dirSign = float2(ds.x < 0 ? -1 : 1, ds.y < 0 ? -1 : 1) * 0.5 / HMres.xy;

    
    while (1.0 - ds.z * sc > t.x && stepCount < debugValues.maxConeStep)
    {
        zTimesSc = ds.z * sc;
#if CONSERVATIVE_STEP
        const float2 p = u + ds.xy * sc;
        const float2 cellCenter = (floor(p*HMres.xy - .5) + 1) / HMres.xy;
        const float2 wall = cellCenter + dirSign;
        const float2 stepToCellBorder = (wall - p) / ds.xy;
        w = min(stepToCellBorder.x, stepToCellBorder.y) + 1e-5;
#endif
        sc += relax * max(w, (1.0 - zTimesSc - t.x) * t.y / (t.y * ds.z + iz));
        tmp = readConeMap(u);
        t = float2(tmp.height, tmp.slope);
        ++stepCount;
    }
    
    HMapIntersection ret = INIT_INTERSECTION;
    ret.last_t = zTimesSc;
    ret.wasHit = (stepCount < debugValues.maxConeStep);
    float tt = ds.z * sc;
    ret.uv = (1 - tt) * u + tt * u2;
    ret.t = tt;
    return ret;
}







struct coneSteppingResult
{
    float2 planeCoord;
    float height;
};

coneSteppingResult parallaxConeStepping(float2 uv, float3 localPos, float3 viewPos, float coneStepScale, int maxSteps)
{
    float3 viewDir = normalize(viewPos - localPos);
    float height = 0.0;

    // Step variables
    float stepSize = 1.0 / maxSteps;
    float2 deltaUV = viewDir.xy * coneStepScale * stepSize;
    float2 currentUV = uv;

    // Perform cone stepping
    for (int i = 0; i < maxSteps; ++i)
    {
        // Read the cone map data at the current UV
        ConeMapData coneData = readConeMap(currentUV);

        // Calculate expected height at this step based on cone slope
        float expectedHeight = height + coneData.slope;

        // If the view depth exceeds the height at the current UV, we break out of the loop
        if (viewDir.z * height > expectedHeight)
        {
            break;
        }

        // Accumulate height and advance UV
        height += coneData.height * stepSize;
        currentUV += deltaUV;
    }

    coneSteppingResult res;
    res.planeCoord = currentUV;
    res.height = height;
    return res;
}


// Ran once per output vertex
output_t main(input_t input) : SV_TARGET
{
    const bool apply_disp = true;
    output_t output;
    
    float2 planeCoord = input.planeCoord;
    const float3 viewPos = camConstants.cameraPos;
    float3 localPos = float3(planeCoord.x, 0, planeCoord.y) + center;
    
    
    coneSteppingResult ret = parallaxConeStepping(planeCoord, localPos, viewPos, float(DISP_MAP_SIZE) / debugValues.patchSizes.r, 1 * debugValues.maxConeStep);
    planeCoord = ret.planeCoord;
    
    localPos = float3(planeCoord.x, ret.height, planeCoord.y) + center;
    float4 grad = readGrad(planeCoord);

    return calculate(grad, input.Position, localPos, planeCoord);
}

