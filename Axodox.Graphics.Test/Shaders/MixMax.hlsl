#include "common.hlsli"

#define LOG2_M 4
#define M (1<<LOG2_M)

//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard 
// Adapted to my use case

RWTexture2D<float2> OutMip1 : register(u0);
RWTexture2D<float2> OutMip2 : register(u1);
RWTexture2D<float2> OutMip3 : register(u2);
RWTexture2D<float2> OutMip4 : register(u3);
Texture2D<float4> SrcMip : register(t0);
SamplerState BilinearClamp : register(s0);

cbuffer CB0 : register(b0)
{
    uint SrcMipLevel; // Texture level of source mip
    uint NumMipLevels; // Number of OutMips to write: [1, 4]
    float2 TexelSize; // 1.0 / OutMip1.Dimensions
}

// The reason for separating channels is to reduce bank conflicts in the
// local data memory controller.  A large stride will cause more threads
// to collide on the same memory bank.
groupshared float gs_mi[64];
groupshared float gs_ma[64];

void StoreColor(uint Index, float2 Color)
{
    gs_mi[Index] = Color.x;
    gs_ma[Index] = Color.y;
}

float2 LoadColor(uint Index)
{
    return float2(gs_mi[Index], gs_ma[Index]);
}
float2 PackColor(float2 val)
{
    return val;
}
float4 LinearInterpolate(float4 Src1, float4 Src2, float4 Src3, float4 Src4)
{
    return 0.25 * (Src1 + Src2 + Src3 + Src4);
}
float Max(float Src1, float Src2, float Src3, float Src4)
{
    return max(max(Src1, Src2), max(Src3, Src4));
}
float Min(float Src1, float Src2, float Src3, float Src4)
{
    return min(min(Src1, Src2), min(Src3, Src4));
}

// all pixels must start a write:
// for texture size N, start N >> 3, N>>3, 1 threads.
// Only works if N is a power of 2.
[numthreads(8, 8, 1)]
void main(uint GI : SV_GroupIndex, uint3 DTid : SV_DispatchThreadID)
{
    float2 UV = TexelSize * (DTid.xy + 0.5);
    float2 Src1 = SrcMip.SampleLevel(BilinearClamp, UV, SrcMipLevel).yy;

    OutMip1[DTid.xy] = Src1;

    // A scalar (constant) branch can exit all threads coherently.
    if (NumMipLevels == 1)
        return;

    // Without lane swizzle operations, the only way to share data with other
    // threads is through LDS.
    StoreColor(GI, Src1);

    // This guarantees all LDS writes are complete and that all threads have
    // executed all instructions so far (and therefore have issued their LDS
    // write instructions.)
    GroupMemoryBarrierWithGroupSync();

    // With low three bits for X and high three bits for Y, this bit mask
    // (binary: 001001) checks that X and Y are even.
    if ((GI & 0x9) == 0)
    {
        float Src2 = LoadColor(GI + 0x01).y;
        float Src3 = LoadColor(GI + 0x08).y;
        float Src4 = LoadColor(GI + 0x09).y;
        Src1.y = Max(Src1.y, Src2, Src3, Src4);
        Src1.x = Min(Src1.x, Src2, Src3, Src4);

        OutMip2[DTid.xy / 2] = PackColor(Src1);
        StoreColor(GI, Src1);
    }

    if (NumMipLevels == 2)
        return;

    GroupMemoryBarrierWithGroupSync();

    // This bit mask (binary: 011011) checks that X and Y are multiples of four.
    if ((GI & 0x1B) == 0)
    {
        float Src2 = LoadColor(GI + 0x02).y;
        float Src3 = LoadColor(GI + 0x10).y;
        float Src4 = LoadColor(GI + 0x12).y;
        Src1.y = Max(Src1.y, Src2, Src3, Src4);
        Src1.x = Min(Src1.x, Src2, Src3, Src4);

        OutMip3[DTid.xy / 4] = PackColor(Src1);
        StoreColor(GI, Src1);
    }

    if (NumMipLevels == 3)
        return;

    GroupMemoryBarrierWithGroupSync();

    // This bit mask would be 111111 (X & Y multiples of 8), but only one
    // thread fits that criteria.
    if (GI == 0)
    {
        float Src2 = LoadColor(GI + 0x04).y;
        float Src3 = LoadColor(GI + 0x20).y;
        float Src4 = LoadColor(GI + 0x24).y;
        Src1.y = Max(Src1.y, Src2, Src3, Src4);
        Src1.x = Min(Src1.x, Src2, Src3, Src4);

        OutMip4[DTid.xy / 8] = PackColor(Src1);
    }
}