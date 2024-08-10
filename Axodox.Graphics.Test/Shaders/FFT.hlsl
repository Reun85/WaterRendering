#include "common.hlsli"
// HLSL compute shader
Texture2D<float2> readbuff : register(t0);
RWTexture2D<float2> writebuff : register(u0);

#define N 1024
#define LOG2_N 10
#define PI		3.1415926535897932
#define TWO_PI	6.2831853071795864


groupshared float2 pingpong[2][N];



/* Should be called as:
    text = the input
    buff = buffer for this shader
output


1. pass: text = readbuff, buff = writebuff
2. pass: text = writebuff, buff = output


*/


// Reverse bits in a number
int ReverseBitfield(int x)
{
    x = ((x >> 1) & 0x55555555) | ((x & 0x55555555) << 1);
    x = ((x >> 2) & 0x33333333) | ((x & 0x33333333) << 2);
    x = ((x >> 4) & 0x0F0F0F0F) | ((x & 0x0F0F0F0F) << 4);
    x = ((x >> 8) & 0x00FF00FF) | ((x & 0x00FF00FF) << 8);
    x = (x >> 16) | (x << 16);
    return x;
}


// LocalGroupSize=N
[numthreads(N, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint3 LTid : SV_GroupThreadID, uint3 GTid : SV_GroupID)
{
    int z = GTid.x;
    int x = LTid.x;


    // Rearrange array for FFT
    // We use f32 => 32

    int nj = (reversebits(x) >> (32 - LOG2_N)) & (N - 1);
    pingpong[0][nj] = readbuff[int2(z, x)];


    
    GroupMemoryBarrierWithGroupSync();

    int src = 0;

    for (int s = 1; s <= LOG2_N; ++s)
    {
        int m = 1L << s; // Height of butterfly group
        int mh = m >> 1; // Half height of butterfly group

        int k = (x * (N / m)) & (N - 1);
        int i = (x & ~(m - 1)); // Starting row of butterfly group
        int j = (x & (mh - 1)); // Butterfly index in group

        float theta = (TWO_PI * float(k)) / float(N);
        float2 W_N_k = float2(cos(theta), sin(theta));
    
        float2 input1 = pingpong[src][i + j + mh];
        float2 input2 = pingpong[src][i + j];

        src = 1 - src;
        pingpong[src][x] = input2 + ComplexMul(W_N_k, input1);

        GroupMemoryBarrierWithGroupSync();
    }

    float2 result = pingpong[src][x];

    writebuff[uint2(x, z)] = result;
}
