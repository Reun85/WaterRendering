
#include "common.hlsli"
struct VS_OUTPUT
{
    float4 Pos : SV_POSITION;
    float3 TexCoord : TEXCOORD0;
};

struct PixelLightData
{
    float4 lightPos;
    float4 lightColor;
};
cbuffer PixelLighting : register(b0)
{
    PixelLightData lights[MAX_LIGHT_COUNT];
    int lightCount;
};

TextureCube skyboxTexture : register(t0);
SamplerState sampleState : register(s0);

float4 main(VS_OUTPUT input) : SV_TARGET
{
//    float3x3 rotate90degrees = float3x3(
//        float3(0, 0, 1),
//        float3(0, 1, 0),
//        float3(-1, 0, 0)
//);
    //float3 dir = mul(rotate90degrees, input.TexCoord);
    float3 dir = input.TexCoord;
   //float3 samp = SampleSkyboxCommon(dir);
    float3 samp = skyboxTexture.Sample(sampleState, dir).xyz;
    float3 sunPos = lights[0].lightPos.xyz;
    float sunRadius = 0.01;

    if (length(
        sunPos - dir) < sunRadius)
    {
        return float4(1, 0, 0, 1);
    }

    return float4(samp, 1);
}