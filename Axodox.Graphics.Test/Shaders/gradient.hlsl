#include "common.hlsli"
Texture2D<float4> displacement : register(t0);
RWTexture2D<float4> gradients : register(u0);
cbuffer Test : register(b9)
{
    ComputeConstants constants;
}


#define N DISP_MAP_SIZE
#define LOG2_M 4
#define M (1<<LOG2_M)
#define MHalf (M>>1)
#define LOG2_N_DIV_M (DISP_MAP_LOG2-LOG2_M)
#define N_DIV_M (1 << LOG2_N_DIV_M)



groupshared float3 cache[M + 2][M + 2];


// The +2 are overhangs
[numthreads(M + 2, M + 2, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint3 LTid : SV_GroupThreadID, uint3 GTid : SV_GroupID)
{

    const float PATCH_SIZE = constants.patchSize;
    const float TILE_SIZE_X2 = PATCH_SIZE * 2.0f / float(DISP_MAP_SIZE);
    const float INV_TILE_SIZE = float(DISP_MAP_SIZE) / PATCH_SIZE;
    
    
    
    int2 groupID = GTid.xy;
    int2 threadID = LTid.xy;

    
    int2 loc = (groupID * M + threadID.xy - 1) & (DISP_MAP_SIZE - 1);
    cache[threadID.x][threadID.y] = displacement[loc].xyz;

        // These threads only exist to read from texture
    if (threadID.x == 0 || threadID.y == 0 || threadID.x == M + 1 || threadID.y == M + 1)
    {
        return;
    }
    GroupMemoryBarrierWithGroupSync();

    
    float3 disp_left = cache[threadID.x - 1][threadID.y];
    float3 disp_right = cache[threadID.x + 1][threadID.y];
    float3 disp_bottom = cache[threadID.x][threadID.y - 1];
    float3 disp_top = cache[threadID.x][threadID.y + 1];
    
    float3 dv = disp_right - disp_left + float3(TILE_SIZE_X2, 0, 0);
    float3 du = disp_top - disp_bottom + float3(0, 0, TILE_SIZE_X2);
    float3 grad = cross(du, dv);
    

    float2 dDx = (dv.xz) * INV_TILE_SIZE / constants.displacementLambda.xz;
    float2 dDy = (du.xz) * INV_TILE_SIZE / constants.displacementLambda.xz;

    float J = dDx.x * dDy.y - dDx.y * dDy.x;


    gradients[loc] = float4(normalize(grad), J);
}
