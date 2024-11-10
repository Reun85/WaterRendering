#include "common.hlsli"

#define LOG2_M 4
#define M (1<<LOG2_M)

// Currently float4, but we only care about .y
Texture2D<float4> srcTexture : register(t0);
RWTexture2D<float4> dstTexture : register(u0);

cbuffer MipInfo : register(b0)
{
    uint srcMipLevel;
};

[numthreads(M, M, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint2 dstCoord = DTid.xy;

    uint2 srcCoord = dstCoord * 2;
    /// its definitely larger than this
    float val = -99999;
    val = max(val, srcTexture.Load(int3(srcCoord, srcMipLevel)).y);
    val = max(val, srcTexture.Load(int3(srcCoord + uint2(1, 0), srcMipLevel)).y);
    val = max(val, srcTexture.Load(int3(srcCoord + uint2(0, 1), srcMipLevel)).y);
    val = max(val, srcTexture.Load(int3(srcCoord + uint2(1, 1), srcMipLevel)).y);

    dstTexture[dstCoord] = float4(0, val, 0, 0);
}