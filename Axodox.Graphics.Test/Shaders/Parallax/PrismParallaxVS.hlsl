#include "../common.hlsli"

cbuffer CameraBuffer : register(b0)
{
    cameraConstants camConstants;
};

cbuffer ModelBuffer : register(b1)
{
    float4x4 mMatrix;
};

struct InstanceData
{
    float2 scaling;
    float2 offset;
};

cbuffer VertexBuffer : register(b2)
{
    InstanceData instances[NUM_INSTANCES];
};

struct input_t
{
    float3 Position : POSITION;
    uint instanceID : SV_InstanceID;
};

struct output_t
{
    float2 planeCoord : PLANECOORD;
    float3 localPosBottom : POSITION0;
    float3 localPosTop : POSITION1;
    float4 ScreenPosBottom : ScreenPos0;
    float4 ScreenPosTop : ScreenPos1;
};

float PrismHeight = 2.0f;

output_t main(input_t input)
{
    output_t output;

    const float2 scaling = instances[input.instanceID].scaling;
    const float2 offset = instances[input.instanceID].offset;
    float2 texCoord = input.Position.xz * scaling + offset;
    output.planeCoord = texCoord;

    output.localPosBottom = float3(texCoord.x, 0, texCoord.y);
    output.localPosTop = output.localPosBottom += float3(0.0f, PrismHeight, 0.0f);
    output.localPosBottom = mul(float4(output.localPosBottom, 1), mMatrix).xyz;
    output.localPosTop = mul(float4(output.localPosTop, 1), mMatrix).xyz;

    output.ScreenPosBottom = mul(float4(output.localPosBottom, 1), camConstants.vpMatrix);
    output.ScreenPosTop = mul(float4(output.localPosTop, 1), camConstants.vpMatrix);
    return output;
}
