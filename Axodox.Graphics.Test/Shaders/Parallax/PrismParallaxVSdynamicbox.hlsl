#include "../common.hlsli"


Texture2D<float4> heightMap1 : register(t6);
Texture2D<float4> heightMap2 : register(t7);
Texture2D<float4> heightMap3 : register(t8);

SamplerState _sampler : register(s0);
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


struct output_t
{
    float3 localPos : POSITION;
    float4 screenPos : SV_Position;
    uint instanceID : SV_InstanceID;
};


struct input_t
{
    uint vertexID : SV_VertexID;
    uint instanceID : SV_InstanceID;
};


cbuffer DebugBuffer : register(b9)
{
    DebugValues debugValues;
}
output_t main(input_t input)
{
    const float eps = 0.1;
    uint vx = input.vertexID;
 
    uint instanceID = input.instanceID;
    output_t output;

    const float2 scaling = instances[instanceID].scaling;
    const float2 offset = instances[instanceID].offset;
    float3 localPos = float3(offset.x, 0, offset.y) + center;
    const float3 cameraPosT = camConstants.cameraPos - localPos;

    uint3 xyz = uint3(vx & 1, (vx & 4) >> 2, (vx & 2) >> 1);

    if (cameraPosT.x > 0)
        xyz.x = 1 - xyz.x;
    if (cameraPosT.y < 0)
        xyz.y = 1 - xyz.y;
    if (cameraPosT.z > 0)
        xyz.z = 1 - xyz.z;

    localPos = float3(xyz) - 0.5;

    localPos.xz = localPos.xz * scaling + offset;
    float height = PrismHeight;

    localPos.y = localPos.y > eps ? height : -10;
    

    output.localPos = localPos + center;
    output.screenPos = mul(float4(output.localPos, 1), camConstants.vpMatrix);
    output.instanceID = instanceID;

    return output;
}
