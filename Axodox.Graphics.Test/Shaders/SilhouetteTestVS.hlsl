struct Vertex
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 textureCoord : TEXCOORD;
};
struct Edge
{
    uint2 vertices;
    int2 faces;
};


StructuredBuffer<Vertex> Vertices : register(t0);
StructuredBuffer<Edge> Edges : register(t1);


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
    //uint edgeVertex = vertexID % 2;
    ////uint2 edge = Edges[instanceID].vertices;
    //uint2 edge = uint2(0, 1);
    
    //// Select the correct vertex of the edge
    //uint vertexIndex = (edgeVertex == 0) ? edge.x : edge.y;
    uint vertexIndex = vertexID % 3;
    float3 position = Vertices[vertexIndex].position;
    

    float4 pos = mul(float4(position, 1), mMatrix);

    float4 screenPosition = mul(pos, camConstants.vpMatrix);
    output.position = screenPosition;
    return output;
}
