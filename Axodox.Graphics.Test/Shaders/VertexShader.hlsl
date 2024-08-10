#include "common.hlsli"



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
    float4 Position : SV_POSITION;
    float3 localPos : POSITION;
    float2 TexCoord : TEXCOORD;
    uint instanceID : InstanceID;
};






output_t main(input_t input)
{
    const float patchSize = PATCH_SIZE;
    output_t output;

    const float2 scaling = instances[input.instanceID].scaling;
    const float2 offset = instances[input.instanceID].offset;
    float2 pos = input.Position.xz * scaling + offset;
    float2 texCoord = (pos / patchSize.xx) - 0.5f.xx;
    
    float4 position = mul(float4(pos.x, 0, pos.y, 1), mMatrix);

    float4 screenPosition = mul(position, camConstants.vpMatrix);
    output.Position = screenPosition;
    output.localPos = position.xyz;
    output.TexCoord = texCoord;
    output.instanceID = input.instanceID;
    return output;
}