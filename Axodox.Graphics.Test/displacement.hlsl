// HLSL compute shader
Texture2D<float2> heightfield : register(t0);
Texture2D<float2> choppyfield : register(t1);
RWTexture2D<float4> displacement : register(u0);


[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint3 LTid : SV_GroupID)
{

    const float lambda = 1.3f;
    int2 loc = int2(DTid.xy);

    // Required due to interval change
    float sign_correction = (((loc.x + loc.y) & 1) == 1) ? -1.0 : 1.0;

    float h = sign_correction * heightfield[loc].x;
    float2 D = sign_correction * choppyfield[loc].xy;

    displacement[loc] = float4(D.x * lambda, h, D.y * lambda, 1.0);
}
