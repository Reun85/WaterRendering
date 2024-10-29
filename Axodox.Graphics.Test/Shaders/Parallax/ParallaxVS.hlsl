
#include "../common.hlsli"



cbuffer CameraBuffer : register(b0)
{
    cameraConstants camConstants;
};
cbuffer ModelBuffer : register(b1)
{
    float3 center;
    float2 scaling;
};
struct output_t
{
    float4 Position : SV_POSITION;
    float2 planeCoord : PLANECOORD;
};

struct input_t
{
    float3 Position : POSITION;
    float4 Normal : NORMAL;
    float2 Texture : TEXCOORD;

};

output_t main(input_t input)
{

    output_t output;

    float2 pos = input.Position.yx * scaling;
    float2 texCoord = pos;
    
    float3 position = float3(pos.x, 0, pos.y) + center;

    float4 screenPosition = mul(float4(position, 1), camConstants.vpMatrix);
    output.Position = screenPosition;
    output.planeCoord = texCoord;
    return output;
}