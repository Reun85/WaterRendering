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






float3 readConeMap(
    float2 uv
)
{
    float height = 0;
    float slope = 999999;
    float mult = 0;
    if (has_flag(debugValues.flags, 3))
    {
        
        float2 t = _coneMap1.SampleLevel(_sampler, GetTextureCoordFromPlaneCoordAndPatch(uv, debugValues.patchSizes.x), 0);
        height += t.x;
        float cmult = debugValues.patchSizes.x;
        if (slope > t.y)
        {
            slope = t.y;
            mult = cmult;
        }
    }
    if (has_flag(debugValues.flags, 4))
    {
        float2 t = _coneMap2.SampleLevel(_sampler, GetTextureCoordFromPlaneCoordAndPatch(uv, debugValues.patchSizes.y), 0);
        height += t.x;
        float cmult = debugValues.patchSizes.y;
        if (slope > t.y)
        {
            slope = t.y;
            mult = cmult;
        }
    }
    if (has_flag(debugValues.flags, 5))
    {
        float2 t = _coneMap3.SampleLevel(_sampler, GetTextureCoordFromPlaneCoordAndPatch(uv, debugValues.patchSizes.z), 0);
        height += t.x;
        float cmult = debugValues.patchSizes.z;
        if (slope > t.y)
        {
            slope = t.y;
            mult = cmult;
        }
    }

    return float3(height, slope, mult);
}

float4 readGrad(float2 uv)
{
    float4 t1 = gradients1.SampleLevel(_sampler, GetTextureCoordFromPlaneCoordAndPatch(uv, debugValues.patchSizes.r), 0);
    float4 t2 = gradients2.SampleLevel(_sampler, GetTextureCoordFromPlaneCoordAndPatch(uv, debugValues.patchSizes.g), 0);
    float4 t3 = gradients3.SampleLevel(_sampler, GetTextureCoordFromPlaneCoordAndPatch(uv, debugValues.patchSizes.b), 0);

    return normalize(t1 + t2 + t3);
}


ConeMarchResult findIntersection_coneStepMapping(float2 uv, float3 viewDirT, uint maxSteps)
{
    float2 HMres = float2(DISP_MAP_SIZE, DISP_MAP_SIZE);
    float prismHeight = 2.;
    float2 u2 = uv - viewDirT.xy / viewDirT.z;

    float3 ds = float3(u2 - uv, 1);
    ds = normalize(ds);
    float w = prismHeight / HMres.x;
    float iz = sqrt(1.0 - ds.z * ds.z); // = length(ds.xz)
    float sc = 0;
    float3 dat = readConeMap(uv);
    int stepCount = 0;
    float zTimesSc = 0.0;

    float2 dirSign = float2(ds.x < 0 ? -1 : 1, ds.y < 0 ? -1 : 1) * 0.5 / HMres.xy;

    float relax = 0.9;
    
    while (prismHeight - ds.z * sc > dat.x && stepCount < maxSteps)
    {
        zTimesSc = ds.z * sc;
#if CONSERVATIVE_STEP
        const float2 p = uv + ds.xy * sc;
        const float2 cellCenter = (floor(p * HMres.xy - .5) + 1) / HMres.xy;
        const float2 wall = cellCenter + dirSign;
        const float2 stepToCellBorder = (wall - p) / ds.xy;
        w = min(stepToCellBorder.x, stepToCellBorder.y) * dat.z + 1e-5;
#endif
        sc += relax * max(w, (prismHeight - zTimesSc - dat.x) * dat.y / (-dat.y / dat.z * ds.z + iz));
        //sc += relax * ConeApprox(-(prismHeight - zTimesSc - dat.x), iz, -ds.z, dat.y / dat.z) * dat.z;
        dat = readConeMap(uv + ds.xy * sc);
        ++stepCount;
    }
    
    ConeMarchResult ret;
    float tt = ds.z * sc;
    ret.uv = (1 - tt) * uv + tt * u2;
    ret.height = tt;
    return ret;
}

output_t main(input_t input) : SV_TARGET
{
    
    const float3 viewPos = camConstants.cameraPos;

    float3 localPos = float3(input.planeCoord.x, 0, input.planeCoord.y) + center;
    float3 viewDirW = normalize(localPos - viewPos);
    float3 T = float3(1, 0, 0);
    float3 B = float3(0, 0, 1);
    float3 N = float3(0, 1, 0);

    float3 viewDirT = mul(viewDirW, transpose(invMat(T, B, N))); // Transform viewDirW to tangent space
 
        
    ConeMarchResult res = findIntersection_coneStepMapping(input.planeCoord, viewDirT, debugValues.maxConeStep);
    localPos = float3(res.uv.x, res.height, res.uv.y);
#ifdef TESTOUT
    output_t output;
    output.albedo = float4(localPos, 1);
    output.normal = float4(OctahedronNormalEncode(float3(0, 1, 0)), 0, 1);
    output.materialValues = float4(0, 0, 0, -1);
    return output;
#endif
    float2 planeCoord = localPos.xz;
    localPos += center;
    
    float4 grad = readGrad(planeCoord);

    // Calculate color based of these attributes
    output_t output = calculate(grad, input.Position, localPos, planeCoord);

    return output;
}
