
#include "common.hlsli"
Texture2D<float4> _texture : register(t0);
TextureCube<float4> _skybox : register(t1);
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



cbuffer MaterialProperties : register(b2)
{
    float3 SurfaceColor;
    float Roughness;
    float SubsurfaceScattering;
    float Sheen;
    float SheenTint;
    float Anisotropic;
    float SpecularStrength;
    float Metallic;
    float SpecularTint;
    float Clearcoat;
    float ClearcoatGloss;
};



float3 PBRShader(float3 localPos, float3 normal, float3 lightDir, float3 lightColor, float lightIntensity)
{
    
}


float3 PBRShader(float3 localPos, float3 normal, int lightIndex)
{
    float3 LightDirection = lights[lightIndex].lightPos.xyz;
    float3 LightColor = lights[lightIndex].lightColor.xyz;
    float LightIntensity = lights[lightIndex].lightColor.w;
    return PBRShader(localPos, normal, LightDirection, LightColor, LightIntensity);
}







float4 main(input_t input, bool isFrontFacing : SV_IsFrontFace) : SV_TARGET
{

    if (has_flag(debugValues.flags, 6))
    {
        float2 texCoord = GetTextureCoordFromPlaneCoordAndPatch(input.planeCoord, debugValues.patchSizes.r);
        float4 text = _texture.Sample(_sampler, texCoord) * float4(debugValues.pixelMult.
        xyz, 1);
      
        return text;
        return Swizzle(text, debugValues.swizzleOrder);
    }
    
    

    

    float4 gradients = input.grad;
    
    
    float3 normal = normalize(gradients.xyz);
    

    const float3 viewVec = camConstants.cameraPos - input.localPos;
    float3 viewDir = normalize(viewVec);
    if (has_flag(debugValues.flags, 24))
    {
        if (dot(normal, viewDir) < 0)
            return float4(1, 0, 0, 1);
        return float4(0, 1, 0, 1);
    }
    if (dot(normal, viewDir) < 0)
    {
        normal *= -1;
    }

    if (has_flag(debugValues.flags, 7))
    {
        return float4(normal, 1);
    }

    float Jacobian = gradients.w;
    if (has_flag(debugValues.flags, 25))
    {
        return float4(Jacobian, Jacobian, Jacobian, 1);
    }


    
    const float depthsqr = dot(viewVec, viewVec);
    const float3 foamColor = debugValues.FoamInfo.xyz;
    const float foamDepthAttenuation = debugValues.FoamInfo.w;
    float foam = 0;
    if (has_flag(debugValues.flags, 2))
    {
        foam = lerp(0.0f, saturate(Jacobian), saturate(pow(depthsqr, foamDepthAttenuation / 2)));
    }


     
    float3 ret = float3(0, 0, 0);

    float3 samp = _skybox.Sample(_sampler, normal).xyz;
    ret += PBRShader(input.localPos, normal, 0);
    ret += PBRShader(input.localPos, normal, normal, samp, Anisotropic);
    ret /= 2;

    ret = lerp(ret, foamColor, saturate(foam));
    
    return float4(ret, 1);
}
