
#include "common.hlsli"
Texture2D<float4> _albedo : register(t0);
Texture2D<float4> _normal : register(t1);
Texture2D<float4> _position : register(t2);
Texture2D<float4> _materialValues : register(t3);
Texture2D<float1> _depthTex : register(t4);
TextureCube<float4> _skybox : register(t5);


// Water
Texture2D<float4> gradients1 : register(t20);
Texture2D<float4> gradients2 : register(t21);
Texture2D<float4> gradients3 : register(t22);


SamplerState _sampler : register(s0);

float LinearizeDepth(float depth, float near, float far)
{
    //return depth / far;
   // return (depth - near) / (far - near);
    return (2.0f * near) / (far + near - depth * (far - near));
    return near / (far - depth * (far - near));
}


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
        return _normalinput;
    }
    if (has_flag(debugValues.flags, 10))
    {
        return -_normalinput;
    }
    if (has_flag(debugValues.flags, 11))
    {
        return float4(localPos, 1);
    }
    if (has_flag(debugValues.flags, 12))
    {
        return _materialValuesinput;
    }
    
    if (has_flag(debugValues.flags, 13))
    {
        float x = LinearizeDepth(depth, 0.01, 1000);
        return float4(x, x, x, 1);
    }

    //if (matId > matIdEPS)
    //{
    //    if (matId < 1 + matIdEPS)
    //    {
    //        const float HighestMax = sqr(debugValues.blendDistances.r);
    //        const float MediumMax = sqr(debugValues.blendDistances.g);
    //        const float LowestMax = sqr(debugValues.blendDistances.b);

    //        float HighestMult = GetMultiplier(0, HighestMax, viewDistanceSqr);
    //        float MediumMult = GetMultiplier(HighestMax, MediumMax, viewDistanceSqr);
    //        float LowestMult = GetMultiplier(MediumMax, LowestMax, viewDistanceSqr);

    //        float highestaccountedfor = 0;
    //        float2 planeCoord = _normalinput.rg;

    
    //        float4 grad = float4(0, 0, 0, 0);

    //// Use displacement
    //        if (has_flag(debugValues.flags, 0))
    //        {
    //            if (has_flag(debugValues.flags, 3) && HighestMult > 0)
    //            {
    //                highestaccountedfor = HighestMax;
    //                float2 texCoord = GetTextureCoordFromPlaneCoordAndPatch(planeCoord, debugValues.patchSizes.r);
    //                return float4(texCoord, 0, 1);
    //                grad += gradients1.SampleLevel(_sampler, texCoord, 0) * HighestMult;
    //            }
    //            if (has_flag(debugValues.flags, 4) && MediumMult > 0)
    //            {
    //                highestaccountedfor = MediumMax;
    //                float2 texCoord = GetTextureCoordFromPlaneCoordAndPatch(planeCoord, debugValues.patchSizes.g);
    //                grad += gradients2.SampleLevel(_sampler, texCoord, 0) * MediumMult;
    //            }
    //            if (has_flag(debugValues.flags, 5) && LowestMult > 0)
    //            {
        
    //                highestaccountedfor = LowestMax;
    //                float2 texCoord = GetTextureCoordFromPlaneCoordAndPatch(planeCoord, debugValues.patchSizes.b);
    //                grad += gradients3.SampleLevel(_sampler, texCoord, 0) * LowestMult;
    //            }
    //        }
    //        else
    //        {
    //            grad = float4(0, 1, 0, 0);
    //        }
    //        if (viewDistanceSqr > highestaccountedfor)
    //        {
    //            grad = float4(0, 1, 0, 0);
    //        }
    //        normal.xyz = grad.xyz;
    //        if (dot(normal, viewDir) < 0)
    //        {
    //            normal *= -1;
    //        }
        
    //    }
    //}

    
    
    normal = normalize(normal);
    
    if (has_flag(debugValues.flags, 7))
    {
        return float4(normal, 1);
    }
   
    
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
    else
    {
        output = albedo * sunIrradiance;
        output += sunIrradiance * specular;
    }
    
    output = output + F * envReflection + AmbientColor;
    
    return float4(output, 1);
}
