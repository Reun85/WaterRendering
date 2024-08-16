
struct VS_OUTPUT
{
    float4 Pos : SV_POSITION;
    float3 TexCoord : TEXCOORD0;
};

TextureCube skyboxTexture : register(t0);
SamplerState sampleState : register(s0);

float4 main(VS_OUTPUT input) : SV_TARGET
{
    return skyboxTexture.Sample(sampleState, input.TexCoord);
}