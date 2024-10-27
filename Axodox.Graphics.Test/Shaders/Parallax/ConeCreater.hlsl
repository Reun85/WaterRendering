#include "../common.hlsli"
Texture2D<float4> displacement : register(t0);
RWTexture2D<float2> cone : register(u10);

//cbuffer Test : register(b9)
//{
//    ComputeConstants constants;
//}


#define N DISP_MAP_SIZE
#define LOG2_M 4
#define M (1<<LOG2_M)
// must be odd
#define BOXSIZE 7
#define MHalf (M>>1)
#define LOG2_N_DIV_M (DISP_MAP_LOG2-LOG2_M)
#define N_DIV_M (1 << LOG2_N_DIV_M)

#define K (BOXSIZE>>1)
#define InnerK (K-1)

groupshared float cache[M + K * 2][M + K * 2];

// The + are overhangs
[numthreads(M + K * 2, M + K * 2, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint3 LTid : SV_GroupThreadID, uint3 GTid : SV_GroupID)
{
    const float PATCH_SIZE = constants.patchSize;
    // Why the div by 2?
    const float TILE_SIZE = PATCH_SIZE / float(DISP_MAP_SIZE) / 2.;

    int2 groupID = GTid.xy;
    int2 threadID = LTid.xy;

    
    int2 loc = (groupID * M + threadID.xy - K) & (DISP_MAP_SIZE - 1);
    float h = displacement.Load(int3(loc, 0)).y;
    // rescale to 0,1    
    h = ((h + 5.) / 10.);
    cache[threadID.x][threadID.y] = h;

    GroupMemoryBarrierWithGroupSync();

    // These threads only exist to read from texture into groupshared mem
    if (threadID.x < K || threadID.y < K || threadID.x >= M + K || threadID.y >= M + K)
    {
        return;
    }

    float ta = 1.;
    float hij = cache[threadID.x][threadID.y];

    [unroll]
    for (int k = -InnerK; k <= InnerK; ++k)
    {
            
        [unroll]
        for (int l = -InnerK; l <= InnerK; ++l)
        {
            int2 loc2 = threadID + int2(k, l);
            float hkl = cache[loc2.x][loc2.y];

            if (hkl - hij <= 0.0f)
            {
                continue;
            }

            // Get neighboring heights
            float h00 = cache[loc2.x][loc2.y];
            float h10 = cache[loc2.x + sign(k)][loc2.y];
            float h01 = cache[loc2.x][loc2.y + sign(l)];
            float h11 = cache[loc2.x + sign(k)][loc2.y + sign(l)];

            // Check if loc2 is limiting
            if (h00 > h10 || h00 > h01 || h10 > h11 || h01 > h11)
            {
                float distance = sqrt(k * k + l * l);
                ta = min(ta, distance / (hkl - hij));
            }
        }
    }
    //ta = ta * N;
    // or is it
    ta = ta * TILE_SIZE;
    // ?


    cone[loc] = float2(hij, ta);
    //cone[loc] = float2(loc.x, loc.y) / float(N);

}
