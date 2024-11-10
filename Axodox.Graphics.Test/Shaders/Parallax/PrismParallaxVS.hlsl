#include "../common.hlsli"

cbuffer CameraBuffer : register(b0)
{
    cameraConstants camConstants;
};

cbuffer ModelBuffer : register(b1)
{
    float4x4 mMatrix;
    float4x4 mINVMatrix;
    float3 center;
    float PrismHeight;
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
    float3 localPos : POSITION;
    float4 screenPos : SV_Position;
    uint instanceID : SV_InstanceID;
};


output_t main(input_t input)
{
    const float eps = 0.1;

    output_t output;

    const float2 scaling = instances[input.instanceID].scaling;
    const float2 offset = instances[input.instanceID].offset;

    float2 texCoord = input.Position.xz * scaling + offset;
    float3 localPos = float3(texCoord.x, input.Position.y > eps ? PrismHeight / 2 : 0, texCoord.y);

    output.localPos = localPos + center;
    output.screenPos = mul(float4(localPos, 1), camConstants.vpMatrix);
    output.instanceID = input.instanceID;

    return output;
}
