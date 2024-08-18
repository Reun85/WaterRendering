#include "common.hlsli"
Texture2D<float4> _texture : register(t0);
SamplerState _sampler : register(s0);


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
    float2 TextureCoord : TEXCOORD;
    float4 grad : GRADIENTS;
};



// Compute the Fresnel-Schlick approximation
float FresnelSchlick(float cosTheta, float3 F0)
{
    float3 F = F0 + (1.0f - F0) * pow(1.0f - cosTheta, 5.0f);
    return dot(F, float3(1.0f, 1.0f, 1.0f)) / 3.0f; // Average of RGB components
}

// GGX Distribution Function
float GGX_D(float3 N, float3 H, float roughness)
{
    float a2 = roughness * roughness;
    float NdotH = max(dot(N, H), 0.0f);
    float NdotH2 = NdotH * NdotH;
    float denominator = (NdotH2 * (a2 - 1.0f) + 1.0f);
    denominator = 3.14159265359 * denominator * denominator;
    return a2 / denominator;
}

// Cook-Torrance BRDF
float3 CookTorranceBRDF(float3 Normal, float3 ViewDir, float3 LightDir, float roughness, float3 F0)
{
    float3 H = normalize(ViewDir + LightDir);
    float3 V = normalize(ViewDir);
    float3 L = normalize(LightDir);

    float D = GGX_D(Normal, H, roughness);
    float F = FresnelSchlick(dot(V, H), F0);
    float G = min(1.0f, min((2.0f * dot(Normal, H) * dot(V, H)) / dot(V, H), (2.0f * dot(Normal, H) * dot(L, H)) / dot(L, H)));
    float numerator = D * F * G;
    float denominator = 4.0f * max(dot(Normal, V), 0.0f) * max(dot(Normal, L), 0.0f);
    
    return numerator / denominator;
}

// The pixel shader function
float3 WaterPixelShader(float3 localPos, float3 normal, float Jacobian)
{
// Colors
    float Roughness = 0.1f;
    const float3 sunColor = float3(1.0, 1.0, 0.47);
    const float3 sunDir = float3(0.45, 0.1, -0.45);
    const float3 waterColor = float3(0.1812f, 0.4678f, 0.5520f);


    
    float3 viewDir = normalize(camConstants.cameraPos - localPos);
    float3 lightDir = normalize(sunDir);

    normal = normalize(normal);

    float3 F0 = float3(0.02f, 0.2f, 0.4f); // F0 for water, you may adjust based on your needs

    // Compute BRDF
    float3 brdf = CookTorranceBRDF(normal, viewDir, lightDir, Roughness, F0);
    
    // Simplified ambient color
    float3 ambientColor = float3(0.1f, 0.2f, 0.3f); // Adjust based on environment

    // Combine with ambient color
    float3 finalColor = ambientColor + brdf * waterColor.rgb;
    
    return finalColor;
}



float4 main(input_t input, bool frontFacing : SV_IsFrontFace) : SV_TARGET
{


    if (has_flag(debugValues.flags, 6))
    {
        float4 text = _texture.Sample(_sampler, input.TextureCoord) * float4(debugValues.pixelMult.
        xyz, 1);
        return Swizzle(text, debugValues.swizzleOrder);
    }
    
    

    

    
    float3 normal = input.grad.xyz;
    normal = frontFacing ? normal : -normal;
    float Jacobian = input.grad.w;
     
    float3 ret = float3(0, 0, 0);
    ret += WaterPixelShader(input.localPos, normal, Jacobian);
    return float4(ret, 1);
}
