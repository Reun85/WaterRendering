#include "common.hlsli"
Texture2D<float2> heightfield : register(t0);
Texture2D<float2> choppyfield : register(t1);
RWTexture2D<float4> displacement : register(u0);

cbuffer Test : register(b9)
{
    ComputeConstants constants;
}

[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint3 LTid : SV_GroupID)
{

    int2 loc = int2(DTid.xy);

    // Required due to interval change
    float sign_correction = (((loc.x + loc.y) & 1) == 1) ? -1.0 : 1.0;

    float h = sign_correction * heightfield[loc].x + 2;
    float2 D = sign_correction * choppyfield[loc].xy;

    // Why are we using float4?
    displacement[loc] = float4(D.x, h, D.y, 0.0) * constants.displacementLambda;
}
