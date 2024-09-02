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



struct PixelLightData
{
    float4 lightPos;
    float4 lightColor;
    float4 AmbientColor;
};
cbuffer PixelLighting : register(b1)
{
    PixelLightData lights[MAX_LIGHT_COUNT];
    int lightCount;
};



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
    float3 _AmbientColor = float3(1, 1, 1);
    float _AmbientMult = 1.0f;
    float NormalDepthAttenuation = 1.f;
    float _HeightModifier = 1;
    float _WavePeakScatterStrength = 1;
    float _ScatterStrength = 1;
    float _ScatterShadowStrength = 1;
};
float SchlickFresnel(float f0, float NdotV)
{
    float x = 1.0 - NdotV;
    float x2 = x * x;
    float x5 = x2 * x2 * x;
    return f0 + (1 - f0) * x5;
}


float SmithMaskingBeckmann(float3 H, float3 S, float roughness)
{
    float hdots = max(0.001f, max(dot(H, S), 0));
    float a = hdots / (roughness * sqrt(1 - hdots * hdots));
    float a2 = a * a;

    return a < 1.6f ? (1.0f - 1.259f * a + 0.396f * a2) / (3.535f * a + 2.181 * a2) : 0.0f;
}

float Beckmann(float ndoth, float roughness)
{
    float exp_arg = (ndoth * ndoth - 1) / (roughness * roughness * ndoth * ndoth);

    return exp(exp_arg) / (PI * roughness * roughness * ndoth * ndoth * ndoth * ndoth);
}


float D_GGX(float NdotH, float roughness)
{
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float denom = NdotH * NdotH * (alpha2 - 1.0) + 1.0;
    return alpha2 / (PI * denom * denom);
}

float G_Smith(float NdotL, float NdotV, float roughness)
{
    float alpha = roughness * roughness;
    float k = alpha * 0.5;
    float G_V = NdotV / (NdotV * (1.0 - k) + k);
    float G_L = NdotL / (NdotL * (1.0 - k) + k);
    return G_V * G_L;
}




output_t main(input_t input, bool frontFacing : SV_IsFrontFace) : SV_TARGET
{

    output_t output;
    
// Colors
    const float3 sunIrradiance = lights[0].lightColor.xyz * lights[0].lightColor.w;
    const float3 sunDir = lights[0].lightPos.xyz;

    
    
    
    
     
	
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
