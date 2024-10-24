#include "../common.hlsli"
Texture2D<float4> displacement : register(t0);
Texture2D<float2> cone : register(t0);

cbuffer Test : register(b9)
{
    ComputeConstants constants;
}


#define N DISP_MAP_SIZE
#define LOG2_M 4
#define M (1<<LOG2_M)
// must be odd
#define BOXSIZE 5
#define MHalf (M>>1)
#define LOG2_N_DIV_M (DISP_MAP_LOG2-LOG2_M)
#define N_DIV_M (1 << LOG2_N_DIV_M)

#define K (BOXSIZE>>1)

groupshared float cache[M + K * 2][M + K * 2];

// The +2 are overhangs
[numthreads(M + K * 2, M + K * 2, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint3 LTid : SV_GroupThreadID, uint3 GTid : SV_GroupID)
{
    const float PATCH_SIZE = constants.patchSize;
    // Why the div by 2?
    const float TILE_SIZE_X2 = PATCH_SIZE * 2. / float(DISP_MAP_SIZE) / 2.;
    const float INV_TILE_SIZE = float(DISP_MAP_SIZE) / PATCH_SIZE;

    int2 groupID = GTid.xy;
    int2 threadID = LTid.xy;

    
    int2 loc = (groupID * M + threadID.xy - K) & (DISP_MAP_SIZE - 1);
    cache[threadID.x][threadID.y] = displacement[loc].y;

    GroupMemoryBarrierWithGroupSync();
        // These threads only exist to read from texture into groupshared mem
    if (threadID.x < K || threadID.y < K || threadID.x >= M + K || threadID.y >= M + K)
    {
        return;
    }
    else
    {
        float ta = 0.;
        float h0 = cache[loc.x][loc.y];

        [unroll]
        for (int i = -K; i <= K; ++i)
        {
            
            [unroll]
            for (int j = -K; j <= K; ++j)
            {
                
                ta = max(ta,
                    (cache[loc.x + i][loc.y + j] - h0) *
                        rsqrt(float(i * i + j * j))
                );
            }
        }
        ta = ta * N;
        // or is it
        // ta = ta * PATCH_SIZE
        // ?


        cone[loc] = float2(h0, ta);
    }
}
