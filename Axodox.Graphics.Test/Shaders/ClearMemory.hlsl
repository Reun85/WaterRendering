#include "common.hlsli"
RWStructuredBuffer<uint> buff : register(u0);



cbuffer Dat : register(b0)
{
    uint number = 1;
}


[numthreads(1, 1, 1)]
void main(
uint3 DTid : SV_DispatchThreadID)
{
    for (int i = 0; i < number; i++)
    {
        buff[i] = 0;
    }
}
