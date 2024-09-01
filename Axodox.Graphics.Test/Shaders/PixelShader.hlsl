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
    float _EnvMapMult = 1;
};
float SchlickFresnel(float f0, float NdotV)
{
    return f0 + (1 - f0) * pow(1.0 - NdotV, 5.0);
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
    
    float3 lightDir = normalize(sunDir);
    float3 halfwayDir = normalize(lightDir + viewDir);
    float depth = input.Screen.z / input.Screen.w;
    float LdotH = DotClamped(lightDir, halfwayDir);
    float VdotH = DotClamped(viewDir, halfwayDir);
    float NdotV = DotClamped(viewDir, normal);
				
    float NdotL = DotClamped(normal, lightDir);
    float NdotH = max(0.0001f, dot(normal, halfwayDir));
				

				

				
    float foam = 0;
    if (has_flag(debugValues.flags, 2))
    {
        foam =
    lerp(0.0f, saturate(Jacobian), pow(depth, foamDepthFalloff));
    }

    normal = lerp(normal, float3(0, 1, 0), pow(depth, NormalDepthAttenuation));



				
    // Make foam appear rougher
    float a = Roughness + foam * foamRoughnessModifier;

    float viewMask = SmithMaskingBeckmann(halfwayDir, viewDir, a);
    float lightMask = SmithMaskingBeckmann(halfwayDir, lightDir, a);
				
    float G = rcp(1 + viewMask + lightMask);
    //float D = Beckmann(NdotH, a);

    //float eta = 1.33f;
    //float R = ((eta - 1) * (eta - 1)) / ((eta + 1) * (eta + 1));
    //float thetaV = acos(viewDir.y);

    //float numerator = pow(1 - dot(normal, viewDir), 5 * exp(-2.69 * a));
    //float F = R + (1 - R) * numerator / (1.0f + 22.7f * pow(a, 1.5f));
    //F = saturate(F);

    float D = D_GGX(NdotH, Roughness);
    float F = SchlickFresnel(0.02, NdotV);
				


    float denom = 4.0 * NdotL * NdotV;
    float3 specular = F * G * D / max(0.001f, denom);

    float3 reflectDir = reflect(-viewDir, normal);
//    float3x3 rotate90degrees = float3x3(
//        float3(0, 0, 1),
//        float3(0, 1, 0),
//        float3(-1, 0, 0)
//);
//    reflectDir = mul(rotate90degrees, reflectDir);
    float3 envReflection = _skybox.Sample(_sampler, reflectDir).rgb * _EnvMapMult;

    //envReflection = SampleSkyboxCommon(reflectDir) * _EnvMapMult;



    float H = max(0.0f, input.localPos.y) * _HeightModifier;
    float3 scatterColor = _ScatterColor;
    float3 ambientColor = _AmbientColor * _AmbientMult;

			

    float k1 = _WavePeakScatterStrength * H * pow(DotClamped(lightDir, -viewDir), 4.0f) * pow(0.5f - 0.5f * NdotL, 3.0f);
    float k2 = _ScatterStrength * pow(DotClamped(viewDir, normal), 2.0f);
    float k3 = _ScatterShadowStrength * max(0, NdotL);

    

    float3 scatter = (k1 + k2) * scatterColor * rcp(1 + lightMask);
    scatter += k3 * scatterColor + ambientColor * rcp(1 + lightMask);

    float3 K1 = k1 * scatterColor * rcp(1 + lightMask);
    float3 K2 = k2 * scatterColor * rcp(1 + lightMask);
    float3 K3 = k3 * scatterColor * rcp(1 + lightMask);
    if (has_flag(debugValues.flags, 26))
    {
        output.albedo =
         float4(K1 * sunIrradiance, 1);
        return output;
    }
    if (has_flag(debugValues.flags, 27))
    {
        output.albedo =
         float4(K2 * sunIrradiance, 1);
        return output;
    }
    if (has_flag(debugValues.flags, 28))
    {
        output.albedo =
         float4(K3 * sunIrradiance, 1);
        return output;
    }
    if (has_flag(debugValues.flags, 29))
    {
        output.albedo =
         float4(K3 * sunIrradiance / rcp(1 + lightMask), 1);
        return output;
    }
    if (has_flag(debugValues.flags, 30))
    {
        output.albedo =
         float4(specular, 1);
        return output;
    }
    if (has_flag(debugValues.flags, 31))
    {
        float x = 6 * viewMask;
        output.albedo =
         float4(x, x, x, 1);
        return output;
    }
				
    float3 albedo = (1 - F) * scatter * sunIrradiance + sunIrradiance * specular + F * envReflection;
    albedo = max(0.0f, albedo);
    albedo = lerp(albedo, _TipColor, saturate(foam));

    output.albedo = float4(albedo, 1);

    output.position = float4(input.localPos, 1);
    output.normal = float4(normal, 1);
    output.materialValues = float4(a, 0, 0, 0);
    return output;
    
}
