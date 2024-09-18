#include "common.hlsli"

struct Vertex
{
    float3 position : POSITION;
    snorm float4 normal : NORMAL;
    unorm float2 texture : TEXCOORD;
};

// Index buffer
struct Face
{
    uint3 indices;
};

struct Edge
{
    uint2 vertices;
    int2 faces;
};


struct DrawIndexedIndirectArgs
{
    uint IndexCountPerInstance;
    uint InstanceCount;
    uint StartIndexLocation;
    int BaseVertexLocation;
    uint StartInstanceLocation;
};


// Vertex Buffer
StructuredBuffer<Vertex> Vertices : register(t0);
// Index Buffer
StructuredBuffer<Face> Faces : register(t1);
RWStructuredBuffer<Edge> Edges : register(u0);
RWStructuredBuffer<DrawIndexedIndirectArgs> EdgeCounter : register(u1);

cbuffer Constants : register(b0)
{
    uint faceCount;
}


cbuffer LightData : register(b1)
{
    SceneLights lightData;
}

#define THREADS_PER_GROUP 256

[numthreads(THREADS_PER_GROUP, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= faceCount)
        return;

    Face face = Faces[DTid.x];
    
    float3 v0 = Vertices[face.indices.x].position;
    float3 v1 = Vertices[face.indices.y].position;
    float3 v2 = Vertices[face.indices.z].position;
    
    float3 edge1 = v1 - v0;
    float3 edge2 = v2 - v0;
    float3 normal = normalize(cross(edge1, edge2));

    
    SingleLightData l = lightData.lights[0];
    float3 toLight = l.lightPos.w == 1 ? (3 * l.lightPos.xyz - v0 - v1 - v2) : l.lightPos.xyz;
    toLight = normalize(toLight);
    
    bool isFrontFacing = dot(normal, toLight) > 0;
    
    // Process each edge of the triangle
    for (int i = 0; i < 1; ++i)
    {
        uint startVertex = face.indices[i];
        uint endVertex = face.indices[(i + 1) % 3];
        
        if (startVertex < endVertex)
        {
            Edge newEdge;
            newEdge.vertices = uint2(startVertex, endVertex);
            newEdge.faces = int2(DTid.x, -1);

            uint edgeIndex;
            InterlockedAdd(EdgeCounter[0].InstanceCount, 1, edgeIndex);
            Edges[edgeIndex] = newEdge;
            
        }
    }
}