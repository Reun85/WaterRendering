// HLSL compute shader
Texture2D<float2> readbuff : register(t0);
RWTexture2D<float2> writebuff : register(u0);

#define N 1024
#define LOG2_N 10
#define PI 3.14159265359
#define TWO_PI 6.28318530718

groupshared float2 pingpong[2][N];

inline float2 ComplexMul(float2 a, float2 b)
{
    return float2(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x);
}


/* Should be called as:
    text = the input and the output as well
    buff = buffer for this shader

1. pass: text = readbuff, buff = writebuff
2. pass: text = writebuff, buff = readbuff

the output is in text

*/


// Reverse bits in a number
int ReverseBitfield(uint x)
{
    x = ((x >> 1) & 0x55555555) | ((x & 0x55555555) << 1);
    x = ((x >> 2) & 0x33333333) | ((x & 0x33333333) << 2);
    x = ((x >> 4) & 0x0F0F0F0F) | ((x & 0x0F0F0F0F) << 4);
    x = ((x >> 8) & 0x00FF00FF) | ((x & 0x00FF00FF) << 8);
    x = (x >> 16) | (x << 16);
    return x;
}

// LocalGroupSize=N
// WorkGroupCount=1 ?
[numthreads(N, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint3 LTid : SV_GroupID)
{
    uint z = GTid.x;
    uint x = LTid.x;

    // Load row/column into shared memory
    pingpong[1][x] = readbuff[int2(z, x)].rg;

    GroupMemoryBarrierWithGroupSync();

    // Rearrange array for FFT
    // We use f32 => 32
    // 
    int nj = (ReverseBitfield(x) >> (32 - LOG2_N)) & (N - 1);
    pingpong[0][nj] = pingpong[1][x];

    GroupMemoryBarrierWithGroupSync();

    // Butterfly stages
    int src = 0;

    for (uint s = 1; s <= LOG2_N; ++s)
    {
        int m = 1L
         << s; // Height of butterfly group
        int mh = m >> 1; // Half height of butterfly group

        int k = (x * (N / m)) & (N - 1);
        int i = (x & ~(m - 1)); // Starting row of butterfly group
        int j = (x & (mh - 1)); // Butterfly index in group

        // WN-k twiddle factor
        float theta = (TWO_PI * float(k)) / N;
        float2 W_N_k = float2(cos(theta), sin(theta));

        float2 input1 = pingpong[src][i + j + mh];
        float2 input2 = pingpong[src][i + j];

        src = 1 - src;
        pingpong[src][x] = input2 + ComplexMul(W_N_k, input1);

        GroupMemoryBarrierWithGroupSync();
    }

    float2 result = pingpong[src][x];

    writebuff[int2(x, z)] = result;
    //writebuff[int2(x, z)] = float2(1, 0);
}
