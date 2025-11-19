
#include "common.hlsli"
Texture2D<float4> _albedo : register(t0);
Texture2D<float4> _normal : register(t1);
Texture2D<float4> _materialValues : register(t3);
Texture2D<float1> _depthTex : register(t4);
TextureCube<float4> _skybox : register(t5);


// Water
Texture2D<float4> gradients1 : register(t20);
Texture2D<float4> gradients2 : register(t21);
Texture2D<float4> gradients3 : register(t22);


SamplerState _sampler : register(s0);



cbuffer DebugBuffer : register(b9)
{
    DebugValues debugValues;
}


cbuffer CameraBuffer : register(b31)
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
    float3 _TipColor;
    float EnvMapMult;
}


struct input_t
{
    //float4 screen : ScreenPos;
    float4 SV_Position : SV_POSITION;
    float2 uv : TEXCOORD;
};





#define RETURNALBEDO_BITMASK (1 << 6 | 1<<24  | 1<<25)
float4 main(input_t input) : SV_TARGET
{

    
    float2 pixelCoordsFloat = input.SV_Position.xy / input.SV_Position.w;
    uint2 pixelCoords = uint2(pixelCoordsFloat);

    uint3 pixelLoadCoords = uint3(pixelCoords, 0);
    
    float4 _albedoinput = _albedo.Load(pixelLoadCoords);
    if ((debugValues.flags & RETURNALBEDO_BITMASK) != 0)
    {
        return float4(_albedoinput.rgb, 1);
    }
   
    float4 _normalinput = _normal.Load(pixelLoadCoords);
    float4 _materialValuesinput = _materialValues.Load(pixelLoadCoords);
    //const float depth = LinearizeDepth(_depthTex.Load(pixelLoadCoords), 0.01, 1000);
    const float depth = _depthTex.Load(pixelLoadCoords);

    float2 ndc = input.uv * 2.0f - 1.0f; // Convert UV [0,1] -> NDC [-1,1]
    float4 clipSpacePos = float4(ndc, depth, 1.0f);

// Apply inverse view-projection matrix to get world space position
    float4 worldPos = mul(clipSpacePos, camConstants.INVvpMatrix);

// Perform perspective divide to get the world-space position
    float3 localPos = hetdiv(worldPos);

  

    float3 albedo = _albedoinput.rgb;
    float3 normal = OctahedronNormalDecode(_normalinput.rg);
    // The albedo.w channel is was stored in only 8bits, while the rest are 16bits!
    const float FresnelFactor = _normalinput.z;
    float Roughness = _materialValuesinput.x;
    float matId = _materialValuesinput.w;
    const float matIdEPS = 0.001f;
    


    float4 lightColor = lights[0].lightColor;
    float4 lightPos = lights[0].lightPos;
    const float3 sunIrradiance = lightColor.xyz * lightColor.w;
    const float3 sunDir = lightPos.xyz;
    const float3 AmbientColor = lights[0].AmbientColor.xyz * lights[0].AmbientColor.w;

    const float3 viewVec = camConstants.cameraPos - localPos;
    const float viewDistanceSqr = dot(viewVec, viewVec);
    float3 viewDir = normalize(viewVec);

    
    if (has_flag(debugValues.flags, 8))
    {
        return _albedoinput;
    }
    if (has_flag(debugValues.flags, 9))
    {
        return float4(normal, 1);
    }
    if (has_flag(debugValues.flags, 10))
    {
        return float4(-normal, 1);
    }
    if (has_flag(debugValues.flags, 11))
    {
        return float4(localPos, 1);
    }
    if (has_flag(debugValues.flags, 12))
    {
        return _materialValuesinput;
    }

    
    
    
    normal = normalize(normal);
    // If displaying gradients
    if (has_flag(debugValues.flags, 7))
    {
        return float4(normal, 1);
    }
   
    
    // TODO: Support point lights
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



    
    float viewMask = SmithMaskingBeckmann(halfwayDir, viewDir, Roughness);
    float lightMask = SmithMaskingBeckmann(halfwayDir, lightDir, Roughness);
				
    float G = rcp(1 + viewMask + lightMask);

    
    float D = Beckmann(NdotH, Roughness);
    float F = SchlickFresnel(FresnelFactor, NdotV);
				


    if (has_flag(debugValues.flags, 20))
    {
        // F = 1;
        F *=
        D = 1;
        G = 1;
    }
    if (has_flag(debugValues.flags, 21))
    {
        F = 1;
        // D = 1;
        G = 1;
    }
    if (has_flag(debugValues.flags, 22))
    {
        F = 1;
        D = 1;
        // G = 1;
    }

    float denom = 4.0 * NdotL * NdotV;

    float3 specular;
    float EPS = 0.0002f;
    if (dot(normal, lightDir) > EPS)
    //if (denom> EPS)
        specular = F * G * D / max(EPS, denom);
    else
        specular = 0;
    specular = max(specular, 0);
    
    
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
            const float foam = _albedoinput.w;
            const float ScatterShadowStrength = _materialValuesinput.z;
            const float HeightModifierAndWavePeakScatterStrength = _materialValuesinput.y;
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
            if (has_flag(debugValues.flags, 27))
            {
                
                float x = dot(normal, lightDir);

                return float4(x, x, x, 1);
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
            output = lerp(output, _TipColor, foam);
            output += sunIrradiance * specular;

        }
        else if (matId < 255 + matIdEPS)
        {
            const float exp = _albedoinput.w;
            output = pow(albedo, exp);
            return float4(output, 1);
        }

    }
    else if (matId < 0)
    {
        return _albedoinput;
    }
    else
    {
        output = albedo * sunIrradiance;
        output += sunIrradiance * specular;
    }
    
    output = output + F * envReflection + AmbientColor;
    
    return float4(output, 1);
}
