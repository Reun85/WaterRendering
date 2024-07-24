Texture2D<float4> _texture : register(t0);
SamplerState _sampler : register(s0);

struct input_t
{
    float4 Screen : SV_Position;
    float2 TextureCoord : TEXCOORD;
    float3 normal : NORMAL;
};

float4 main(input_t input) : SV_TARGET
{
    float4 samp = _texture.Sample(_sampler, input.TextureCoord);
    //if (samp.x < 0.001 && samp.x > -0.001)
    //    return float4(1, 0, 0, 1);
    
    float4 ret;
    //float4 ret = float4(samp.y, 0, 0, 1);
    ret = samp;
     
    ret = float4(1, 1, 1, 1);
    return ret;
}