#include "common.hlsli"

struct Vertex
{
    float3 position : POSITION;
    // Due to compression these are useless data
    uint2 normal : NORMAL;
    half texCoord : TEXCOORD;
};


// Index buffer
struct Face
{
    uint3 indices;
};

struct Edge
{
    //float4 vert;
    int2 vertices;
    // faces.y == -1 means its empty
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
uint hash(uint2 v)
{
    return v.x ^ (v.y * 0x1f1f1f1f);
}

#define THREADS_PER_GROUP 16
#define MAX_FACES_PER_GROUP 32
#define SILHOUETTE_MARKER_MASK 0x80000000

groupshared Vertex s_Vertices[MAX_FACES_PER_GROUP * 3];
groupshared Face s_Faces[MAX_FACES_PER_GROUP];
groupshared uint s_EdgeCount;
groupshared Edge s_Edges[MAX_FACES_PER_GROUP * 3];


[numthreads(THREADS_PER_GROUP, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint3 Gid : SV_GroupID)
{
    float3 LightPosition = lightData.lights[0].lightPos.xyz;
    
    uint groupFaceCount = min(MAX_FACES_PER_GROUP, faceCount - Gid.x * MAX_FACES_PER_GROUP);
    
    uint i;
    // Load faces and vertices into shared memory
    for (i = GTid.x; i < groupFaceCount; i += THREADS_PER_GROUP)
    {
        uint faceIdx = Gid.x * MAX_FACES_PER_GROUP + i;
        s_Faces[i] = Faces[faceIdx];
        
        [unroll]
        for (uint j = 0; j < 3; ++j)
        {
            s_Vertices[i * 3 + j] = Vertices[s_Faces[i].indices[j]];
        }
    }

    
    // Set default state for s_Edges
    for ( i = GTid.x; i < MAX_FACES_PER_GROUP * 3; i+= THREADS_PER_GROUP)
    {
        s_Edges[i].faces.y = -1;
    }
    
    
    if (GTid.x == 0)
        s_EdgeCount = 0;
    
    GroupMemoryBarrierWithGroupSync();
    
    // Process faces
    for (int faceIndex = GTid.x; faceIndex < groupFaceCount; faceIndex += THREADS_PER_GROUP)
    {
        //float3 v0 = s_Vertices[faceIndex * 3].position;
        //float3 v1 = s_Vertices[faceIndex * 3 + 1].position;
        //float3 v2 = s_Vertices[faceIndex * 3 + 2].position;

        float3 v0 = s_Vertices[s_Faces[faceIndex].indices[0]].position;
        float3 v1 = s_Vertices[s_Faces[faceIndex].indices[1]].position;
        float3 v2 = s_Vertices[s_Faces[faceIndex].indices[2]].position;
        
        float3 normal = normalize(cross(v1 - v0, v2 - v0));
        //float3 normal = normalize(cross(v2-v0,v1 - v0));
        bool isFrontFacing = dot(normal, normalize(LightPosition)) > 0;
        
        // Process edges
        [unroll]
        for (uint e = 0; e < 3; ++e)
        {
            uint startVertex = s_Faces[faceIndex].indices[e];
            uint endVertex = s_Faces[faceIndex].indices[(e + 1) % 3];
            
            if (startVertex < endVertex)
            {
                // Hash table kind of
                uint edgeHash = hash(uint2(startVertex, endVertex)) % (MAX_FACES_PER_GROUP * 3);
                
                // Potential hash conflict
                bool inserted = false;
                while (!inserted)
                {
                    int originalFace;
                    InterlockedCompareExchange(s_Edges[edgeHash].faces.y, -1, faceIndex, originalFace);
                    
                    if (originalFace == -1)
                    {
                        if (isFrontFacing)
                        {
                            s_Edges[edgeHash].faces.x = faceIndex | SILHOUETTE_MARKER_MASK;
                            s_Edges[edgeHash].vertices = int2(startVertex, endVertex);
                            InterlockedAdd(s_EdgeCount, 1);
                        }
                        inserted = true;
                    }
                    // if the hash bucket is occupied by another face
                    else if (originalFace != faceIndex)
                    {
                        if (isFrontFacing)
                        {
                            // Its added multiple times, its not part of the silhouette
                            s_Edges[edgeHash].faces.x &= (SILHOUETTE_MARKER_MASK - 1);
                        }
                        inserted = true;
                    }

                   // Hash conflict 
                    edgeHash = (edgeHash + 1) % (MAX_FACES_PER_GROUP * 3);
                }
            }
        }
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    // For testing
    s_EdgeCount = MAX_FACES_PER_GROUP * 3;
    
    uint globalOffset;
    if (GTid.x == 0)
    {
        InterlockedAdd(EdgeCounter[0].InstanceCount, s_EdgeCount, globalOffset);
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    // Output silhouette edges
    for (i = GTid.x; i < MAX_FACES_PER_GROUP*3; i += THREADS_PER_GROUP)
    {
        if ((s_Edges[i].faces.y != -1) && (s_Edges[i].faces.x & SILHOUETTE_MARKER_MASK)!=0)
        {
            uint idx;
            InterlockedAdd(s_EdgeCount, -1, idx);
            uint outputIndex = globalOffset + idx;
            Edges[outputIndex] = s_Edges[i];
            Edges[outputIndex].faces.x &= (SILHOUETTE_MARKER_MASK - 1); // Clear silhouette flag
        }
    }
}