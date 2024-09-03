
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
struct output_t
{
    float4 albedo;
    float4 normal;
    float4 position;
    float4 materialValues;
};

output_t main(VS_OUTPUT input) : SV_TARGET
{
    output_t output;
//    float3x3 rotate90degrees = float3x3(
//        float3(0, 0, 1),
//        float3(0, 1, 0),
//        float3(-1, 0, 0)
//);
    //float3 dir = mul(rotate90degrees, input.TexCoord);
    float3 dir = input.TexCoord;
   //float3 samp = SampleSkyboxCommon(dir);
    float4 samp = skyboxTexture.Sample(sampleState, dir);
    float3 sunPos = lights[0].lightPos.xyz;
    float sunRadius = 0.005;

    if (length(
        sunPos - dir) < sunRadius)
    {
        samp = float4(1, 0, 0, 1);
    }

    output.albedo = samp;
    output.materialValues = float4(0, 0, 0, 2);
    return output;
}