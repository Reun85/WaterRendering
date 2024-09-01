
#include "common.hlsli"
Texture2D<float4> _albedo : register(t0);
Texture2D<float4> _normal : register(t1);
Texture2D<float4> _position : register(t2);
Texture2D<float4> _materialValues : register(t3);
Texture2D<float> _depthTex : register(t4);

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



cbuffer PixelLightData : register(b1)
{
    float4 lightPos;
    float4 lightColor;
};



struct input_t
{
    float4 screen : SV_Position;
    float2 texCoord : TEXCOORD;
};

float4 main(input_t input) : SV_TARGET
{
    float4 _albedoinput = _albedo.Sample(_sampler, input.texCoord);
    float4 _normalinput = _normal.Sample(_sampler, input.texCoord);
    float4 _positioninput = _position.Sample(_sampler, input.texCoord);
    float4 _materialValuesinput = _materialValues.Sample(_sampler, input.texCoord);
    float _depth = _depthTex.Sample(_sampler, input.texCoord);

    float3 albedo = _albedoinput.rgb;
    float3 normal = _normalinput.rgb;
    float3 localPos = _positioninput.rgb;
    // The albedo.w channel is was stored in only 8bits, while the rest are 16bits!
    float3 other = float3(_albedoinput.w, _normalinput.w, _positioninput.w);
    float Roughness = _materialValuesinput.x;
    float val1 = _materialValuesinput.y;
    float val2 = _materialValuesinput.z;
    float val3 = _materialValuesinput.w;
    


    const float3 sunIrradiance = lightColor.xyz * lightColor.w;
    const float3 sunDir = lightPos.xyz;

    const float3 viewVec = camConstants.cameraPos - localPos;
    float3 viewDir = normalize(viewVec);
    
    
    float3 lightDir = normalize(sunDir);
    float3 halfwayDir = normalize(lightDir + viewDir);
    float LdotH = DotClamped(lightDir, halfwayDir);
    float VdotH = DotClamped(viewDir, halfwayDir);
    float NdotV = DotClamped(viewDir, normal);
				
    float NdotL = DotClamped(normal, lightDir);
    float NdotH = max(0.0001f, dot(normal, halfwayDir));
				

    
    
    float4 output = float4(albedo, 1);
    
    return output;
}
