#include "common.hlsli"

struct input_t
{
    float3 position : POSITION;
    float4 normal : NORMAL;
    float2 texture : TEXCOORD;
};

struct output_t
{
    float4 position : SV_Position;
    float2 textureCoord : TEXCOORD;
};

output_t main(input_t input)
{

    output_t output;
    output.position = float4(input.position, 1);
    output.textureCoord = input.texture;
    return output;
}
