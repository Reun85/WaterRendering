// HLSL compute shader
Texture2D<float2> tilde_h0 : register(t0);
Texture2D<float> frequencies : register(t1);
RWTexture2D<float2> tilde_h : register(u0);
RWTexture2D<float2> tilde_D : register(u1);


#define N 1024
#define M N

cbuffer Constants : register(b0)
{
    float time;
}


inline float2 ComplexMul(float2 a, float2 b)
{
    return float2(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x);
}



// What would the group sizes be?
[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint3 LTid : SV_GroupID)
{
 // SV_DISPATCHTHREADID is the globalPos 
    int2 loc1 = int2(DTid.xy);
    int2 loc2 = int2(N - loc1.x, M - loc1.y);

    // Load initial spectrum
    float2 h0_k = tilde_h0[loc1].rg;
    float2 h0_mk = tilde_h0[loc2].rg;
    float w_k = frequencies[loc1].r;

    // Height spectrum
    float cos_wt = cos(w_k * time);
    float sin_wt = sin(w_k * time);

    float2 h_tk = ComplexMul(h0_k, float2(cos_wt, sin_wt)) + ComplexMul(float2(h0_mk.x, -h0_mk.y), float2(cos_wt, -sin_wt));
    

    // "Choppy" 
    float2 k = float2(N / 2 - loc1.x, M / 2 - loc1.y);
    float2 nk = normalize(k);

    float2 Dt_x = float2(h_tk.y * nk.x, -h_tk.x * nk.x);
    float2 iDt_z = float2(h_tk.x * nk.y, h_tk.y * nk.y);

    // Write result
    tilde_h[loc1] = h_tk;
    tilde_D[loc1] = Dt_x + iDt_z;
}
