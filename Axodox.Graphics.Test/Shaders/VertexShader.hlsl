#include "common.hlsli"

cbuffer CameraBuffer : register(b0)
{
    cameraConstants camConstants;
};
cbuffer ModelBuffer : register(b1)
{
    float4x4 mMatrix;
};
cbuffer VertexBuffer : register(b2)
{
    float2 scaling;
    float2 offset;
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
};






output_t main(input_t input)
{
    const float patchSize = 20;
    const float planeSize = 1000;
    const float2 PlaneBottomLeft = float2(-planeSize / 2, -planeSize / 2);
    const float2 PlaneTopRight = float2(planeSize / 2, planeSize / 2);
    output_t output;

    float2 pos = input.Position.xz * scaling + offset;
    float2 texCoord = (pos - PlaneBottomLeft) / (PlaneTopRight - PlaneBottomLeft);
    
    float4 position = mul(float4(pos.x, 0, pos.y, 1), mMatrix);

    float4 screenPosition = mul(position, camConstants.vpMatrix);
    output.Position = screenPosition;
    output.localPos = position.xyz;
    output.TexCoord = texCoord;
    return output;
}