
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



// Utility Functions
float3 SchlickFresnel(float3 f0, float3 f90, float VdotH)
{
    return f0 + (f90 - f0) * pow(1.0 - VdotH, 5.0);
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

float3 F_Schlick(float3 f0, float3 f90, float VdotH)
{
    return f0 + (f90 - f0) * pow(1.0 - VdotH, 5.0);
}

// Disney BRDF Functions
float3 DisneyDiffuse(float NdotL, float NdotV, float LdotH, float LdotN)
{
    float energyBias = 0.5 * Roughness;
    float energyFactor = lerp(1.0, 1.0 / 1.51, Roughness);

    float fd90 = energyBias + 2.0 * LdotH * LdotH * Roughness;
    float lightScatter = 1.0 + (fd90 - 1.0) * pow(1.0 - NdotL, 5.0);
    float viewScatter = 1.0 + (fd90 - 1.0) * pow(1.0 - NdotV, 5.0);

    return lightScatter * viewScatter * energyFactor;
}

float3 Subsurface(float NdotL, float NdotV, float LdotH)
{
    float FL = 1.0 - pow(1.0 - NdotL, 5.0);
    float FV = 1.0 - pow(1.0 - NdotV, 5.0);
    return FL * FV * (1.0 / PI);
}

float3 SheenBRDF(float3 normal, float3 viewDir, float3 lightDir, float3 H)
{
    float NdotL = saturate(dot(normal, lightDir));
    float NdotV = saturate(dot(normal, viewDir));
    float HdotV = saturate(dot(H, viewDir));

    float3 sheenColor = lerp(float3(1.0, 1.0, 1.0), SurfaceColor, SheenTint);

    return Sheen * sheenColor * SchlickFresnel(float3(0.0, 0.0, 0.0), sheenColor, HdotV);
}

// Pixel Shader
float3 PBRShader(float3 localPos, float3 normal, float3 LightDirection, float3 LightColor, float LightIntensity)
{
    
    
    normal = normalize(normal);
    float3 viewDir = normalize(camConstants.cameraPos - localPos);
    float3 lightDir = normalize(LightDirection);

    // Sample base color
    float3 baseColor = SurfaceColor;

    float3 H = normalize(lightDir + viewDir);

    float NdotL = saturate(dot(normal, lightDir));
    float NdotV = saturate(dot(normal, viewDir));
    float NdotH = saturate(dot(normal, H));
    float LdotH = saturate(dot(lightDir, H));

    // Diffuse
    float3 diffuseColor = baseColor;
    float3 diffuse = (1.0 - Metallic) * DisneyDiffuse(NdotL, NdotV, LdotH, NdotL) * diffuseColor;

    // Subsurface Scattering
    float3 subsurface = SubsurfaceScattering * Subsurface(NdotL, NdotV, LdotH) * diffuseColor;

    // Specular
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), diffuseColor, Metallic);
    float3 F = F_Schlick(F0, float3(1.0, 1.0, 1.0), NdotH);
    float D = D_GGX(NdotH, Roughness);
    float G = G_Smith(NdotL, NdotV, Roughness);

    float3 specular = F * G * D / (4.0 * NdotL * NdotV);

    // Sheen
    float3 sheen = SheenBRDF(normal, viewDir, lightDir, H);

    // Clearcoat
    float3 clearcoat = Clearcoat * D_GGX(NdotH, ClearcoatGloss) * lerp(0.04, 1.0, NdotH);

    // Reflection
    float3 reflectionVector = reflect(-viewDir, normal);
    float4 reflectionColor = _skybox.Sample(_sampler, reflectionVector);
    reflectionColor *= SpecularStrength;
    
    
    // Combine all contributions
    float3 lighting = LightColor * LightIntensity * (diffuse + subsurface + sheen + specular + clearcoat * NdotL) + reflectionColor.xyz;


    // Final color output
    return lighting;
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
    
    

    

    
    
    float3 normal = input.grad.xyz;
    // Why is it backwards??????????
    //normal = isFrontFacing ? normal : -normal;

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
        return float4(normal , 1);
    }

    float Jacobian = input.grad.w;
    if (has_flag(debugValues.flags, 25))
    {
        return float4(Jacobian, Jacobian, Jacobian, 1);
    }


    
    const float depthsqr = dot(viewVec, viewVec);
    const float3 foamColor = debugValues.FoamInfo.xyz;
    const float foamDepthAttenuation = debugValues.FoamInfo.w;
    float foam=0;
    if (has_flag(debugValues.flags, 2))
    {
     foam = lerp(0.0f, saturate(Jacobian), pow(depthsqr, foamDepthAttenuation/2));
    }


     
    float3 ret = float3(0, 0, 0);

    float3 samp = _skybox.Sample(_sampler, normal).xyz;
    ret += PBRShader(input.localPos, normal, 0);
    //ret += PBRShader(input.localPos, normal, normal, samp,1);
    //ret /= 2;

    ret = lerp(ret, foamColor, saturate(foam));
    
    return float4(ret, 1);
}
