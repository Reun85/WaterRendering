#include "common.hlsli"
Texture2D<float4> _texture : register(t0);
SamplerState _sampler : register(s0);

#define PI 3.14159265359

cbuffer CameraBuffer : register(b0)
{
    cameraConstants camConstants;
}


cbuffer DebugBuffer : register(b9)
{
    DebugValues debugValues;
}




struct input_t
{
    float4 Screen : SV_Position;
    float3 localPos : POSITION;
    float2 planeCoord : PLANECOORD;
    float4 grad : GRADIENTS;
};

struct output_t
{
    float4 albedo;
    float4 normal;
    float4 position;
    float4 materialValues;
};



cbuffer PSProperties : register(b2)
{
    float3 SurfaceColor = float3(0.109, 0.340, 0.589);
    float Roughness = 0.285;
    float3 _TipColor = float3(0.8f, 0.9f, 1.0f);
    float foamDepthFalloff = 1.0f;
    float3 _ScatterColor = float3(1, 1, 1);
    float foamRoughnessModifier = 1.0f;
    float NormalDepthAttenuation = 1.f;
    float _HeightModifier = 1;
    float _WavePeakScatterStrength = 1;
    float _ScatterStrength = 1;
    float _ScatterShadowStrength = 1;
};



output_t main(input_t input, bool frontFacing : SV_IsFrontFace) : SV_TARGET
{

    output_t output;
    
	
    if (has_flag(debugValues.flags, 6))
    {
        float2 texCoord = GetTextureCoordFromPlaneCoordAndPatch(input.planeCoord, debugValues.patchSizes.r);
        float4 text = _texture.Sample(_sampler, texCoord) * float4(debugValues.pixelMult.
        xyz, 1);
      
        output.albedo = text;
        return output;
    }
        
    float3 normal = normalize(input.grad.xyz);
    
    const float3 viewVec = camConstants.cameraPos - input.localPos;
    float3 viewDir = normalize(viewVec);

    if (has_flag(debugValues.flags, 24))
    {
        output.albedo =
         float4(0, 1, 0, 1);
        if (dot(normal, viewDir) < 0)
            output.albedo = float4(1, 0, 0, 1);
        
        return output;
    }

    if (dot(normal, viewDir) < 0)
    {
        normal *= -1;
    }

    if (has_flag(debugValues.flags, 7))
    {
        output.albedo =
    float4(normal, 1);
        return output;
    }

    float Jacobian = input.grad.w;
    if (has_flag(debugValues.flags, 25))
    {
        output.albedo =
         float4(Jacobian, Jacobian, Jacobian, 1);
        return output;
    }
    
    float depth = input.Screen.z / input.Screen.w;

				

				
    float foam = 0;
    if (has_flag(debugValues.flags, 2))
    {
        foam =
    lerp(0.0f, saturate(Jacobian), pow(depth, foamDepthFalloff));
    }

    normal = lerp(normal, float3(0, 1, 0), pow(depth, NormalDepthAttenuation));



				
    // Make foam appear rougher
    float a = Roughness + foam * foamRoughnessModifier;

    float3 scatterColor = _ScatterColor;
   
    				
    float3 albedo = scatterColor;
    foam = 0;
    albedo = lerp(albedo, _TipColor, saturate(foam));

    //low
    output.albedo = float4(albedo, 1);
    //high
    output.position = float4(input.localPos, _HeightModifier * _WavePeakScatterStrength);
    //high
    output.normal = float4(normal, 1);
    //low
    output.materialValues = float4(a, _ScatterStrength, _ScatterShadowStrength, 1);
    return output;
    
}
