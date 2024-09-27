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
