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







float ConeApprox(
float y, float sinB, float cosB, float tanA)
{
    return tanA * y / (sinB - cosB * tanA);
}

float dcell(float p, float v, float size)
{
    float x = floor(p * size + 0.5) + 0.5 * sign(v);
    float vr = 1. / v;
    float sizer = 1. / size;
    return (x) * vr * sizer - p * vr;
}
// assume rectangular image
float dcell(float2 p, float2 v, float size)
{
    return min(dcell(p.x, v.x, size), dcell(p.y, v.y, size));
}
//https://github.com/Bundas102/robust-cone-map/tree/master
// for planes with normal (0,1,0)
// and centered around (0,0,0)

//#define TESTOUT

float3 ParallaxConemarch(float3 viewPos, float3 localPos)
{
    float3 rayp = viewPos;
    float3 rayv = localPos - rayp;
    float t = length(rayv);
    rayv /= t;
    
    
    float acc = 0;
    
    const uint maxSteps =
        50;
        //debugValues.maxConeStep;
    const float map_height = 1.;
    float dt;
    float3 p = localPos;
    float sinB = sqrt(1. - rayv.y * rayv.y);
    int i;
    for (i = 0; i < maxSteps; ++i)
    {
        float3 dat = readConeMap(p.xz);

        float h = dat.x;
        float tan = dat.y;
        float mult = dat.z;
        float y = h - p.y;
        if (y < -0.001)
        {
            break;
        }


        dt = max(
         ConeApprox(y, sinB, rayv.y, tan / mult) * mult,

        // ensure we atleast reach a new data holding texel.
        dcell(p.xz, rayv.xz, DISP_MAP_SIZE) * debugValues.patchSizes.r
        );
        //dt = max(ConeApprox(y, sinB, rayv.y, tan * mult), dcell(p.xz, rayv.xz, DISP_MAP_SIZE) * mult);


        acc += dt;
        t = t - dt;
        p = rayp + t * rayv;

    }
#ifdef TESTOUT
    float x = acc / 10;
    return float3(x, x, x);
#endif
    return p;
}


// Ran once per output vertex
output_t main(input_t input) : SV_TARGET
{
    
    float2 planeCoord = input.planeCoord;
    const float3 viewPos = camConstants.cameraPos;
       
    //output_t output;
    //output.albedo = float4(readConeMap(float3(planeCoord.x, 0, planeCoord.y).xz) / 10., 1, 1);
        
    float3 localPos = ParallaxConemarch(viewPos - center, float3(planeCoord.x, 0, planeCoord.y));
#ifdef TESTOUT
    output_t output;
    output.albedo = float4(localPos, 1);
    output.normal = float4(OctahedronNormalEncode(float3(0, 1, 0)), 0, 1);
    output.materialValues = float4(0, 0, 0, -1);
    return output;
#endif
    planeCoord = localPos.xz;
    localPos += center;
    
    float4 grad = readGrad(planeCoord);

    // Calculate color based of these attributes
    return calculate(grad, input.Position, localPos, planeCoord);
}
