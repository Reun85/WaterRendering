Texture2D<float4> _texture : register(t0);
SamplerState _sampler : register(s0);



cbuffer Constants : register(b0)
{
    float4 cameraPos;
    float4 mult;
    bool useTexture;
}

struct input_t
{
    float4 Screen : SV_Position;
    float4 localPos : POSITION;
    float2 TextureCoord : TEXCOORD;
    float3 normal : NORMAL;
};

float4 main(input_t input) : SV_TARGET
{

    const bool actually = false;
    if (useTexture && actually)
    {
        return _texture.Sample(_sampler, input.TextureCoord) * float4(mult.xyz, 1);
    }

    
    
    

    
    float3 La = float3(1.0, 1.0, 1.0);
    float3 Ld = float3(1.0, 1.0, 1.0);
    float3 Ls = float3(1.0, 1.0, 1.0);

    float lightConstantAttenuation = 1.0;
    float lightLinearAttenuation = 0.0;
    float lightQuadraticAttenuation = 0.0;


    float3 Ka = float3(1.0, 1.0, 1.0f);
    float3 Kd = float3(1.0, 1, 1);
    float3 Ks = float3(1.0, 1, 1);

    float Shininess = 1.0;
    
    
    const float3 sunColor = float3(1.0, 1.0, 0.47);
    const float3 sundir = float3(0.45, 0.7, 0);
    
    float4 ret = float4(0.1812f,
    0.4678f, 0.5520f, 1);
     

    float3 normal = normalize(input.normal.xzy);
	
    float LightDistance = 0.0;
	

    float3 ToLight = normalize(sundir);
    float3 Attenuation = 0;
    float3 Diffuse = 0;
    float3 Specular = 0;
	
    Attenuation = 1.0 / (lightConstantAttenuation + lightLinearAttenuation * LightDistance + lightQuadraticAttenuation * LightDistance * LightDistance);
	
    float3 Ambient = La * Ka;

    float DiffuseFactor = max(dot(ToLight, normal), 0.0) * Attenuation;
    Diffuse = DiffuseFactor * Ld * Kd;

    float3 viewDir = normalize(cameraPos.xyz - input.localPos.xyz / input.localPos.w);
    float3 reflectDir = reflect(-ToLight, normal);
	
    float SpecularFactor = pow(max(dot(viewDir, reflectDir), 0.0), Shininess) * Attenuation;
    Specular = SpecularFactor * Ls * Ks;

    ret *= float4(Ambient + Diffuse + Specular, 1.0);
    


    return ret;
}