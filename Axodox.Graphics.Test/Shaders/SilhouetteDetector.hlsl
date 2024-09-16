#include "common.hlsli"

struct Vertex
{
    float3 position;
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
    
    // Calculate face normal
    float3 v0 = Vertices[face.indices.x].position;
    float3 v1 = Vertices[face.indices.y].position;
    float3 v2 = Vertices[face.indices.z].position;
    
    float3 edge1 = v1 - v0;
    float3 edge2 = v2 - v0;
    float3 normal = normalize(cross(edge1, edge2));

    
    SingleLightData l = lightData.lights[0];
    float3 toLight = l.lightPos.w == 1 ? (3 * l.lightPos.xyz - v0 - v1 - v2) : l.lightPos.xyz;
    toLight = normalize(toLight);
    
    // Check if face is front-facing relative to light
    bool isFrontFacing = dot(normal, toLight) > 0;
    
    // Process each edge of the triangle
    for (int i = 0; i < 3; ++i)
    {
        uint startVertex = face.indices[i];
        uint endVertex = face.indices[(i + 1) % 3];
        
        if (startVertex < endVertex)
        {
            Edge newEdge;
            newEdge.vertices = uint2(startVertex, endVertex);
            newEdge.faces = int2(DTid.x, -1); // Current face index and placeholder for adjacent face
            
            uint edgeIndex;
            InterlockedAdd(EdgeCounter[0].InstanceCount, 1, edgeIndex);
            
            // Try to add the edge or update existing edge
            InterlockedCompareExchange(Edges[edgeIndex].faces.y, -1, DTid.x,
                                       Edges[edgeIndex].faces.y);
            
            // If this is a new edge, store it
            if (Edges[edgeIndex].faces.y == -1)
            {
                Edges[edgeIndex] = newEdge;
            }
            else
            {
                // Check if this is a silhouette edge
                int otherFace = Edges[edgeIndex].faces.y;
                if ((isFrontFacing && otherFace < DTid.x) ||
                    (!isFrontFacing && otherFace > DTid.x))
                {
                    // Mark as silhouette edge by setting MSB of faces.x
                    Edges[edgeIndex].faces.x |= 0x80000000;
                }
            }
        }
    }
}