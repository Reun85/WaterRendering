StructuredBuffer<float3> Vertices : register(t0);
StructuredBuffer<uint2> Edges : register(t1);


#include "common.hlsli"

cbuffer CameraBuffer : register(b0)
{
    cameraConstants camConstants;
};
cbuffer ModelBuffer : register(b1)
{
    float4x4 mMatrix;
};
struct VSOutput
{
    float4 position : SV_POSITION;
};

// Vertex Shader
VSOutput main(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
    VSOutput output;
    
    // Each instance is an edge, and we draw 2 vertices per edge
    uint edgeVertex = vertexID % 2;
    uint2 edge = Edges[instanceID];
    
    // Select the correct vertex of the edge
    uint vertexIndex = (edgeVertex == 0) ? edge.x : edge.y;
    float3 position = Vertices[vertexIndex];
    

    float4 pos = mul(float4(position, 1), mMatrix);

    float4 screenPosition = mul(pos, camConstants.vpMatrix);
    output.position = screenPosition;
    return output;
}
