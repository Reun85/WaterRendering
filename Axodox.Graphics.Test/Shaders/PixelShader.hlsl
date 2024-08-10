#include "common.hlsli"
Texture2D<float4> _texture : register(t0);
SamplerState _sampler : register(s0);


cbuffer CameraBuffer : register(b0)
{
    cameraConstants camConstants;
}


cbuffer Constants : register(b1)
{
    float4 mult;
    uint4 swizzleorder;
    int useTexture;
}

struct input_t
{
    float4 Screen : SV_Position;
    float3 localPos : POSITION;
    float2 TextureCoord : TEXCOORD;
    float4 normal : Variable;
};




float4 main(input_t input) : SV_TARGET
{


    if (useTexture == 0)
    {
        float4 text = _texture.Sample(_sampler, input.TextureCoord) * float4(mult.xyz, 1);
        return Swizzle(text, swizzleorder);
    }

    
// Colors
    const float3 sunColor = float3(1.0, 1.0, 0.47);
    const float3 sunDir = float3(0.45, 0.1, -0.45);
    const float3 waterColor = float3(0.1812f, 0.4678f, 0.5520f);

    
    // Light
    float3 La = float3(0.3, 0.3, 0.4); // Ambient light from the sky
    float3 Ld = float3(1.0, 0.95, 0.8);
    float3 Ls = float3(1.0, 1.0, 0.3);
    const float lightConstantAttenuation = 1.0;
// These are for point lights
    const float lightLinearAttenuation = 0.0;
    const float lightQuadraticAttenuation = 0.0;

    // Water

    float3 Ka = waterColor * 3;
    float3 Kd = float3(0.2, 0.2, 0.2) * 4;
    float3 Ks = float3(0.9, 0.9, 0.9) * 2;
    float Shininess = 64.0;

    
    
    //if (cameraPos.x < 0.0 == input.localPos.x < 0 && cameraPos.z < 0 == input.localPos.z < 0)
    //if (input.localPos.x < 0 == sunDir.x < 0 && input.localPos.z < 0 == sunDir.z < 0)
    //{
    //    return float4(1, 0, 0, 1);
    //}n
     

    float3 normal = normalize(input.normal.xyz);
	
    float LightDistance = 0.0;
	

    float3 ToLight = normalize(sunDir);
	
    // Will be 1
    float Attenuation = 1.0 / (lightConstantAttenuation + lightLinearAttenuation * LightDistance + lightQuadraticAttenuation * LightDistance * LightDistance);
	
    float3 Ambient = La * Ka;

    float DiffuseFactor = max(dot(ToLight, normal), 0.0) * Attenuation;
    float3 Diffuse = DiffuseFactor * Ld * Kd;

    float3 viewDir = normalize(camConstants.cameraPos.xyz - (input.localPos.xyz));
    float3 reflectDir = reflect(-ToLight, normal);
	
    float SpecularFactor = pow(max(dot(viewDir, reflectDir), 0.0), Shininess) * Attenuation;
    float3 Specular = SpecularFactor * Ls * Ks;

    
    
    float3 lighting = Ambient + Diffuse + Specular;
    float3 ret = waterColor * lighting;
    


    return float4(ret, 1);
}