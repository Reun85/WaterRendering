// 20 bytes
struct VertexRaw
{
    // 4 bytes * 5
    uint data[5];
};

struct Vertex
{
    float3 position : POSITION;
};
Vertex FromRaw(VertexRaw v)
{
    Vertex r;
    r.position = float3(asfloat(v.data[0]), asfloat(v.data[1]), asfloat(v.data[2]));
    return r;
}


// Index buffer
struct Face
{
    uint3 indices;
};

struct Edge
{
    //float4 vert;
    uint2 vertices;
    // -1 means its empty
    int faceID;

    // 0
    int counted;
};

StructuredBuffer<VertexRaw> Vertices : register(t0);
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
    uint edgeVertex = vertexID % 2;
    uint2 edge = Edges[instanceID].vertices;
    //uint2 edge = uint2(0, 1);
    
    // Select the correct vertex of the edge
    uint vertexIndex = (edgeVertex == 0) ? edge.x : edge.y;
    Vertex v = FromRaw(Vertices[vertexIndex]);
    float3 position = v.position;
    

    float4 pos = mul(float4(position, 1), mMatrix);

    float4 screenPosition = mul(pos, camConstants.vpMatrix);
    output.position = screenPosition;
    return output;
}
