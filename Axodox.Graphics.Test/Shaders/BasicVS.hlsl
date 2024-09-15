
#include "common.hlsli"



cbuffer CameraBuffer : register(b0)
{
    cameraConstants camConstants;
};
cbuffer ModelBuffer : register(b1)
{
    float4x4 mMatrix;
};
struct input_t
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 textureCoord : TEXCOORD;
};
struct output_t
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 textureCoord : TEXCOORD;
};




output_t main(input_t input)
{
    output_t output;

    float4 position = mul(float4(input.position, 1), mMatrix);

    float4 screenPosition = mul(position, camConstants.vpMatrix);
    output.position = screenPosition;
    output.normal = input.normal;
    output.textureCoord = input.textureCoord;
    return output;
}
