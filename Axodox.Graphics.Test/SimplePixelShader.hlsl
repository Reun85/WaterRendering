Texture2D<float2> _texture : register(t0);
SamplerState _sampler : register(s0);

struct input_t
{
    float4 Screen : SV_Position;
    float2 TextureCoord : TEXCOORD;
    float3 normal : NORMAL;
};

float4 main(input_t input) : SV_TARGET
{
    float2 samp = _texture.Sample(_sampler, input.TextureCoord);
    //if (samp.x < 0.001 && samp.x > -0.001)
    //    return float4(1, 0, 0, 1);
    return float4(samp, 0, 1);
}