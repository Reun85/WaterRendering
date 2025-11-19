#include "common.hlsli"

struct DrawIndexedIndirectArgs
{
    uint IndexCountPerInstance;
    uint InstanceCount;
    uint StartIndexLocation;
    int BaseVertexLocation;
    uint StartInstanceLocation;
};

RWStructuredBuffer<DrawIndexedIndirectArgs> buff : register(u0);


[numthreads(1, 1, 1)]
void main(
uint3 DTid : SV_DispatchThreadID)
{
    buff[0].IndexCountPerInstance = 2;
    buff[0].InstanceCount = 0;
    buff[0].StartIndexLocation = 0;
    buff[0].BaseVertexLocation = 0;
    buff[0].StartInstanceLocation = 0;
}
