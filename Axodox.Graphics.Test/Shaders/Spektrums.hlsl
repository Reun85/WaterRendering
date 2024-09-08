#include "common.hlsli"
// HLSL compute shader
Texture2D<float2> tilde_h0 : register(t0);
Texture2D<float> frequencies : register(t1);
RWTexture2D<float2> tilde_h : register(u0);
RWTexture2D<float2> tilde_D : register(u1);


#define N DISP_MAP_SIZE

cbuffer Constants : register(b0)
{
    TimeConstants timeData;
}



#define LOG2_M 4
#define M (1<<LOG2_M)
#define MHalf (M>>1)
#define LOG2_N_DIV_M (DISP_MAP_LOG2-LOG2_M)
#define N_DIV_M (1 << LOG2_N_DIV_M)



groupshared float2 cache[M][M];
// What would the group sizes be?
[numthreads(MHalf, M, 2)]
//[numthreads(M, M, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint3 LTid : SV_GroupID)
{
    int2 groupID = LTid.xy;
    int2 threadID = GTid.xy;
    bool mirroredInMiddle = GTid.z == 1;

    if (mirroredInMiddle)
    {
        groupID = N_DIV_M - 1 - groupID;
        threadID = M - 1 - threadID;
    }

    
    int2 loc1 = groupID * M + threadID.xy;
    int2 loc2 = int2(N - 1 - loc1.x, N - 1 - loc1.y);

    float2 h0_k = tilde_h0[loc1].rg;
    cache[threadID.x][threadID.y] = h0_k;
    GroupMemoryBarrierWithGroupSync();
    float2 h0_mk = cache[M - 1 - threadID.x][M - 1 - threadID.y];

    // The above solution mimics this implementation using shared memory => +10fps.
    /*
     [numthreads(M,M,1)]

    int2 loc1 = DTid.xy;
    int2 loc2 = int2(N - 1 - loc1.x, N - 1 - loc1.y);
    // Load initial spectrum
    float2 h0_k = tilde_h0[loc1].rg;
    float2 h0_mk = tilde_h0[loc2].rg;
*/
    
    
    float w_k = frequencies[loc1].r;

    // Height spectrum
    const float time = timeData.timeSinceLaunch;
    float cos_wt = cos(w_k * time);
    float sin_wt = sin(w_k * time);

    float2 h_tk = ComplexMul(h0_k, float2(cos_wt, sin_wt)) + ComplexMul(float2(h0_mk.x, -h0_mk.y), float2(cos_wt, -sin_wt));
    

    // "Choppy" 
    float2 k = float2(N / 2 - loc1.x, N / 2 - loc1.y);
    float2 nk = float2(0, 0);

    
    if (dot(k, k) > 1e-12)
        nk = normalize(k);
    

    float2 Dt_x = float2(h_tk.y * nk.x, -h_tk.x * nk.x);
    float2 iDt_z = float2(h_tk.x * nk.y, h_tk.y * nk.y);

    // Write result
    tilde_h[loc1] = h_tk;
    tilde_D[loc1] = Dt_x + iDt_z;
}
