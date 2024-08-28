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
float SchlickFresnel(float3 normal, float3 viewDir)
{
				// 0.02f comes from the reflectivity bias of water kinda idk it's from a paper somewhere i'm not gonna link it tho lmaooo
    return 0.02f + (1 - 0.02f) * (pow(1 - max(dot(normal, viewDir), 0), 5.0f));
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
float4 main(input_t input, bool frontFacing : SV_IsFrontFace) : SV_TARGET
{


    // Why is it backwards??????????
    //normal = isFrontFacing ? normal : -normal;
    
// Colors
    const float3 sunColor = lights[0].lightColor;
    const float3 sunDir = lights[0].lightPos;
    const float3 waterColor = SurfaceColor;

    
    // Other constants
    const float3 _DiffuseReflectance = float3(0.1f, 0.1f, 0.1f);
    const float _FresnelNormalStrength = 1.0f;
    const float _FresnelShininess = 5.0f;
    const float _FresnelBias = 0.02f;
    const float _FresnelStrength = 1.0f;
    const float3 _FresnelColor = float3(0.6f, 0.8f, 1.0f);
    const float3 _SpecularReflectance = float3(0.2f, 0.2f, 0.2f);
    const float _SpecularNormalStrength = 0.5f;
    const float _Shininess = 50.0f;
    const float3 _LightColor0 = float3(1.0f, 1.0f, 1.0f);
    const float3 _TipColor = float3(0.8f, 0.9f, 1.0f);
    const float _TipAttenuation = 10.0f;
    const float3 _Ambient = float3(0.05f, 0.05f, 0.1f);
    const float _NormalStrength = 1;

    float normalDepthFalloff = 1.0f;


    float shininess = 1.0f;

    float specularNormalStrength = 1.0f;

    float fresnelBias = 0.0f;

    float fresnelStrength = 1.0f;

    float fresnelShininess = 5.0f;

    float fresnelNormalStrength = 1.0f;

    float bubbleDensity = 1.0f;

    float roughness = 0.1f;

    float foamRoughnessModifier = 1.0f;

    float heightModifier = 1.0f;

    float wavePeakScatterStrength = 1.0f;
    
    float scatterStrength = 1.0f;

    float scatterShadowStrength = 1.0f;

    float environmentLightStrength = 1.0f;


    float foamBias = -0.5f;

    float foamThreshold = 0.0f;

    float foamAdd = 0.5f;

    float foamDecayRate = 0.05f;

    float foamDepthFalloff = 1.0f;

    const float _NormalDepthAttenuation = 1.0f;
    
    
    
     
	
    if (has_flag(debugValues.flags, 6))
    {
        float2 texCoord = GetTextureCoordFromPlaneCoordAndPatch(input.planeCoord, debugValues.patchSizes.r);
        float4 text = _texture.Sample(_sampler, texCoord) * float4(debugValues.pixelMult.
        xyz, 1);
      
        return text;
        return Swizzle(text, debugValues.swizzleOrder);
    }
    
    

    

    
    
    float3 normal = normalize(input.grad.xyz);
    

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

    float Jacobian = input.grad.w;
    if (has_flag(debugValues.flags, 25))
    {
        return float4(Jacobian, Jacobian, Jacobian, 1);
    }
    
    float3 lightDir = -normalize(sunDir);
    float3 halfwayDir = normalize(lightDir + viewDir);
    float depth = input.Screen.z / input.Screen.w;
    float LdotH = DotClamped(lightDir, halfwayDir);
    float VdotH = DotClamped(viewDir, halfwayDir);
				
				

				

				
    float foam = lerp(0.0f, saturate(Jacobian), pow(depth, foamDepthFalloff));

    float3 macroNormal = float3(0, 1, 0);
    float3 mesoNormal = normal;
    mesoNormal = normalize(lerp(macroNormal, mesoNormal, pow(saturate(depth), _NormalDepthAttenuation)));

    float NdotL = DotClamped(mesoNormal, lightDir);

				
    float a = roughness + foam * foamRoughnessModifier;
    float ndoth = max(0.0001f, dot(mesoNormal, halfwayDir));

    float viewMask = SmithMaskingBeckmann(halfwayDir, viewDir, a);
    float lightMask = SmithMaskingBeckmann(halfwayDir, lightDir, a);
				
    float G = rcp(1 + viewMask + lightMask);

    float eta = 1.33f;
    float R = ((eta - 1) * (eta - 1)) / ((eta + 1) * (eta + 1));
    float thetaV = acos(viewDir.y);

    float numerator = pow(1 - dot(mesoNormal, viewDir), 5 * exp(-2.69 * a));
    float F = R + (1 - R) * numerator / (1.0f + 22.7f * pow(a, 1.5f));
    F = saturate(F);
				
    float3 specular = _SunIrradiance * F * G * Beckmann(ndoth, a);
    specular /= 4.0f * max(0.001f, DotClamped(macroNormal, lightDir));
    specular *= DotClamped(mesoNormal, lightDir);

    float3 envReflection = texCUBE(_EnvironmentMap, reflect(-viewDir, mesoNormal)).rgb;
    envReflection *= _EnvironmentLightStrength;

    float H = max(0.0f, displacementFoam.y) * _HeightModifier;
    float3 scatterColor = _ScatterColor;
    float3 bubbleColor = _BubbleColor;
    float bubbleDensity = _BubbleDensity;

				
    float k1 = _WavePeakScatterStrength * H * pow(DotClamped(lightDir, -viewDir), 4.0f) * pow(0.5f - 0.5f * dot(lightDir, mesoNormal), 3.0f);
    float k2 = _ScatterStrength * pow(DotClamped(viewDir, mesoNormal), 2.0f);
    float k3 = _ScatterShadowStrength * NdotL;
    float k4 = bubbleDensity;

    float3 scatter = (k1 + k2) * scatterColor * _SunIrradiance * rcp(1 + lightMask);
    scatter += k3 * scatterColor * _SunIrradiance + k4 * bubbleColor * _SunIrradiance;

				
    float3 output = (1 - F) * scatter + specular + F * envReflection;
    output = max(0.0f, output);
    output = lerp(output, _FoamColor, saturate(foam));

    return float4(output, 1);
    
}
