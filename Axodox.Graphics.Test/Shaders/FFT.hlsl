#include "common.hlsli"
Texture2D<float2> readbuff : register(t0);
RWTexture2D<float2> writebuff : register(u0);

#define N DISP_MAP_SIZE
#define LOG2_N DISP_MAP_LOG2


groupshared float2 pingpong[2][N];



/* Should be called as:
    text = the input
    buff = buffer for this shader
output


1. pass: text = readbuff, buff = writebuff
2. pass: text = writebuff, buff = output


*/


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
    
        GroupMemoryBarrierWithGroupSync();
        float2 input1 = pingpong[src][i + j + mh];
        float2 input2 = pingpong[src][i + j];

        src = 1 - src;
        pingpong[src][x] = input2 + ComplexMul(W_N_k, input1);

    }

    float2 result = pingpong[src][x];

    writebuff[uint2(x, z)] = result;
}
