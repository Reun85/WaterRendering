Texture2D _texture : register(t0);
SamplerState _sampler : register(s0);

struct input_t
{
    float4 Screen : SV_Position;
    float2 TextureCoord : TEXCOORD;
    float3 normal : NORMAL;
};

float4 main(input_t input) : SV_TARGET
{
    return _texture.Sample(_sampler, input.TextureCoord);
}