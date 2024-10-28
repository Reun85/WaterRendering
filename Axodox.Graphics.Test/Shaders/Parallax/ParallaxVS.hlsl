
#include "../common.hlsli"



cbuffer CameraBuffer : register(b0)
{
    cameraConstants camConstants;
};
cbuffer ModelBuffer : register(b1)
{
    float2 scaling;
    float3 center;
};
struct output_t
{
    float4 Position : SV_POSITION;
    float3 localPos : POSITION;
    float2 planeCoord : PLANECOORD;
};


output_t main(uint vertexID : SV_VertexID)
{
    float2 planeCoords[6] =
    {
        float2(-1, 1), // Top-left
    float2(1, -1), // Bottom-right
    float2(1, 1), // Top-right

    float2(-1, 1), // Top-left (repeat to start new triangle)
    float2(-1, -1), // Bottom-left
    float2(1, -1) // Bottom-right
    };

    output_t output;

    float2 pos = planeCoords[vertexID] * scaling;
    float2 texCoord = pos;
    
    float3 position = float3(pos.x, 0, pos.y) + center;

    float4 screenPosition = mul(float4(position, 1), camConstants.vpMatrix);
    output.Position = screenPosition;
    output.localPos = position.xyz;
    output.planeCoord = texCoord;
    return output;
}