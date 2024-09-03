
#include "common.hlsli"
Texture2D<float4> _albedo : register(t0);
Texture2D<float4> _normal : register(t1);
Texture2D<float4> _position : register(t2);
Texture2D<float4> _materialValues : register(t3);
Texture2D<float> _depthTex : register(t4);
TextureCube<float4> _skybox : register(t5);


SamplerState _sampler : register(s0);

#define PI 3.14159265359



cbuffer DebugBuffer : register(b9)
{
    DebugValues debugValues;
}


cbuffer CameraBuffer : register(b0)
{
    cameraConstants camConstants;
}



struct PixelLightData
{
    float4 lightPos;
    float4 lightColor;
    float4 AmbientColor;
};
cbuffer Lighting : register(b1)
{
    PixelLightData lights[MAX_LIGHT_COUNT];
    int lightCount;
};

cbuffer DSBuffer : register(b2)
{
    float EnvMapMult;
}


struct input_t
{
    float4 screen : SV_Position;
    float2 texCoord : TEXCOORD;
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

    return a < 1.6f ? (1.0f - 1.259f * a + 0.396f * a2) / (3.535f * a + 2.181 * a2) : 1e-4f;
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



#define RETURNALBEDO_BITMASK (1 << 6 | 1 << 7 | 1<<24  | 1<<25)
float4 main(input_t input) : SV_TARGET
{
    float4 _albedoinput = _albedo.Sample(_sampler, input.texCoord);
    if ((debugValues.flags & RETURNALBEDO_BITMASK) != 0)
    {
        return float4(_albedoinput.rgb, 1);
    }
    
    float4 _normalinput = _normal.Sample(_sampler, input.texCoord);
    float4 _positioninput = _position.Sample(_sampler, input.texCoord);
    float4 _materialValuesinput = _materialValues.Sample(_sampler, input.texCoord);
    float _depth = _depthTex.Sample(_sampler, input.texCoord);

    float3 albedo = _albedoinput.rgb;
    float3 normal = _normalinput.rgb;
    float3 localPos = _positioninput.rgb;
    // The albedo.w channel is was stored in only 8bits, while the rest are 16bits!
    float Roughness = _materialValuesinput.x;
    float matId = _materialValuesinput.w;
    const float matIdEPS = 0.001f;
    


    float4 lightColor = lights[0].lightColor;
    float4 lightPos = lights[0].lightPos;
    const float3 sunIrradiance = lightColor.xyz * lightColor.w;
    const float3 sunDir = lightPos.xyz;
    const float3 AmbientColor = lights[0].AmbientColor.xyz * lights[0].AmbientColor.w;

    const float3 viewVec = camConstants.cameraPos - localPos;
    float3 viewDir = normalize(viewVec);
    
    
    float3 lightDir = normalize(sunDir);
    float3 halfwayDir = normalize(lightDir + viewDir);
    float LdotH = DotClamped(lightDir, halfwayDir);
    float VdotH = DotClamped(viewDir, halfwayDir);
    float NdotV = DotClamped(viewDir, normal);
				
    float NdotL = DotClamped(normal, lightDir);
    float NdotH = max(0.0001f, dot(normal, halfwayDir));
				

    float3 reflectDir = reflect(-viewDir, normal);
    float4 envReflectioninp = _skybox.Sample(_sampler, reflectDir);
    float3 envReflection = pow(envReflectioninp.rgb * EnvMapMult, envReflectioninp.w);



    // Old code
    //float D = Beckmann(NdotH, a);

    //float eta = 1.33f;
    //float R = ((eta - 1) * (eta - 1)) / ((eta + 1) * (eta + 1));
    //float thetaV = acos(viewDir.y);

    //float numerator = pow(1 - dot(normal, viewDir), 5 * exp(-2.69 * a));
    //float F = R + (1 - R) * numerator / (1.0f + 22.7f * pow(a, 1.5f));
    //F = saturate(F);

    float viewMask = SmithMaskingBeckmann(halfwayDir, viewDir, Roughness);
    float lightMask = SmithMaskingBeckmann(halfwayDir, lightDir, Roughness);
				
    float G = rcp(1 + viewMask + lightMask);

    
    float D = D_GGX(NdotH, Roughness);
    float F = SchlickFresnel(0.02, NdotV);
				


    float denom = 4.0 * NdotL * NdotV;
    float3 specular = F * G * D / max(0.001f, denom);

    
    if (has_flag(debugValues.flags, 30))
    {
        return
         float4(specular, 1);
    }
    
    

    float3 output = float3(0, 0, 0);
    
    if (matId > matIdEPS)
    {
        if (matId < 1 + matIdEPS)
        {
            const float ScatterStrength = _materialValuesinput.y;
            const float ScatterShadowStrength = _materialValuesinput.z;
            const float HeightModifierAndWavePeakScatterStrength = _positioninput.w;
            // water
            float H = max(0.0f, localPos.y) * HeightModifierAndWavePeakScatterStrength;
            const float3 scatterColor = albedo;
			

            const float k1 = H * pow(DotClamped(lightDir, -viewDir), 4.0f) * pow(0.5f - 0.5f * NdotL, 3.0f);
            const float k3 = ScatterShadowStrength * max(0, NdotL);


            if (has_flag(debugValues.flags, 26))
            {
                float3 K1 = k1 * scatterColor * rcp(1 + lightMask);
                return float4(K1 * sunIrradiance, 1);
            }
            if (has_flag(debugValues.flags, 28))
            {
                float3 K3 = k3 * scatterColor * rcp(1 + lightMask);
                return float4(K3 * sunIrradiance, 1);
            }
            if (has_flag(debugValues.flags, 29))
            {
                float3 K3 = k3 * scatterColor * rcp(1 + lightMask);
                return float4(K3 * sunIrradiance * rcp(1 + lightMask), 1);
            }
            if (has_flag(debugValues.flags, 31))
            {
                float x = 6 * lightMask;
                return float4(x, x, x, 1);
            }

    

            float3 scatter = k1 * scatterColor;
            scatter += k3 * scatterColor;
            output = (1 - F) * scatter * sunIrradiance;
            output += sunIrradiance * specular;

        }
        else if (matId < 255 + matIdEPS)
        {
            const float exp = _albedoinput.w;
            output = pow(albedo, exp);
            return float4(output, 1);
        }

    }
    else
    {
        output = albedo * sunIrradiance;
        output += sunIrradiance * specular;
    }
    
    output = output + F * envReflection + AmbientColor;
    
    return float4(output, 1);
}
