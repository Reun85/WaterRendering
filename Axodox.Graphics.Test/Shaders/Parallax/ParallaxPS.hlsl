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






float2 readConeMap(
    float2 uv
)
{
    float height = 0;
    float slope = 999999;
    if (has_flag(debugValues.flags, 3))
    {
        
        float2 t = _coneMap1.SampleLevel(_sampler, GetTextureCoordFromPlaneCoordAndPatch(uv, debugValues.patchSizes.r), 0);
        height += t.x;
        slope = min(slope, t.y);
    }
    if (has_flag(debugValues.flags, 4))
    {
        float2 t = _coneMap2.SampleLevel(_sampler, GetTextureCoordFromPlaneCoordAndPatch(uv, debugValues.patchSizes.g), 0);
        height += t.x;
        slope = min(slope, t.y);
    }
    if (has_flag(debugValues.flags, 5))
    {
        float2 t = _coneMap3.SampleLevel(_sampler, GetTextureCoordFromPlaneCoordAndPatch(uv, debugValues.patchSizes.b), 0);
        height += t.x;
        slope = min(slope, t.y);
    }


    return float2(height, slope);
}

float4 readGrad(float2 uv)
{
    float4 t1 = gradients1.SampleLevel(_sampler, GetTextureCoordFromPlaneCoordAndPatch(uv, debugValues.patchSizes.r), 0);
    float4 t2 = gradients2.SampleLevel(_sampler, GetTextureCoordFromPlaneCoordAndPatch(uv, debugValues.patchSizes.g), 0);
    float4 t3 = gradients3.SampleLevel(_sampler, GetTextureCoordFromPlaneCoordAndPatch(uv, debugValues.patchSizes.b), 0);

    return t1 + t2 + t3;
}







struct Ray
{
    float3 p, v;
};

struct Intersection
{

    float t;
    float2 uv;
};
float ray_coneapprox(
float z, float sinB, float cosB, float cotA)
{
    return z / (sinB * cotA - cosB);
}
// for planes with normal (0,1,0)
// and centered around (0,0,0)
float3 ParallaxConemarch(float3 viewPos, float3 localPos)
{
    Ray ray;
    ray.p = viewPos;
    ray.v = normalize(localPos - ray.p);
    float t = (localPos - ray.p).x / ray.v.x;
    
    
    
    const float map_height = 5.;
    float t_all = map_height / abs(ray.v.y);
    t -= t_all;
    float dt = t_all / float(debugValues.maxConeStep);
    float3 p = ray.p + t * ray.v;
    float sinB = sqrt(1. - ray.v.y * ray.v.y);
    for (int i = 0; i < debugValues.maxConeStep; ++i)
    {
        float2 dat = readConeMap(p.xz);

        float L = dat.y;
        float y = dat.x - p.y;
        if (y < .0)
            break;
        t += ray_coneapprox(abs(y), sinB, ray.v.y, L) + dt;
        p = ray.p + t * ray.v;
    }
    //vec2 tt = vec2(pt.t-dt,pt.t); //(x1,x2)
    //vec2 ff = vec2(f(ray,tt.x,p),f(ray,tt.y,p)); //f(x1),f(x2)
    //for (int i=0; i<2; ++i)
    //{
    //    tt = vec2(tt.y,tt.y - ff.y*(tt.y-tt.x)/(ff.y-ff.x));
    //    ff = vec2(ff.y,f(ray,tt.x,p));
    //}
    return p;
}


// Ran once per output vertex
output_t main(input_t input) : SV_TARGET
{
    const bool apply_disp = true;
    output_t output;
    
    float2 planeCoord = input.planeCoord;
    const float3 viewPos = camConstants.cameraPos;
       
    //output.albedo = float4(readConeMap(float3(planeCoord.x, 0, planeCoord.y).xz) / 10., 1, 1);
    //output.normal = float4(OctahedronNormalEncode(float3(0, 1, 0)), 0, 1);
    //output.materialValues = float4(0, 0, 0, 0);
    //return output;
    
    float3 localPos = ParallaxConemarch(viewPos - center, float3(planeCoord.x, 0, planeCoord.y));
    planeCoord = localPos.xz;
    localPos += center;
    
    float4 grad = readGrad(planeCoord);

    return calculate(grad, input.Position, localPos, planeCoord);
}

