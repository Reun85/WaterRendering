
Texture2D _heightmap : register(t0);
SamplerState _sampler : register(s0);
cbuffer HullBuffer : register(b0)
{
    // zneg,xneg, zpos, xpos
    float4 TessellationFactor;
};

// --------------------------------------
// Hull Shader
// --------------------------------------


// Input
struct HS_INPUT_PATCH
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD;
};

struct HS_OUTPUT_PATCH
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD;
};

// Ran once per control of input patch
[domain("quad")]
[partitioning("fractional_even")]
[patchconstantfunc("HSConstantFunction")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
HS_OUTPUT_PATCH main(InputPatch<HS_INPUT_PATCH, 4> patch, uint i : SV_OutputControlPointID, uint PatchID : SV_PrimitiveID)
{
    HS_OUTPUT_PATCH output;
    output.Position = patch[i].Position;

    output.TexCoord = patch[i].TexCoord;
    
    return
output;
}


inline float ToNearest2Power(float u)
{
    return u;
    uint v = (uint) u;
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return (float)
    v;
}


inline float dostuff(float4 a, float4 b)
{
    float u = length(float3(a.x, a.y, a.z) / a.w - float3(b.x, b.y, b.z) / b.w);
    u = u * 32;
    
    if (u < 1)
    {
        return 1;
    }
    if (u < 2)
    {
        return 2;
    }
    if (u < 4)
    {
        return 4;
    }
    if (u < 8)
    {
        return 8;
    }
    if (u < 16)
    {
        return 16;
    }
    if (u < 32)
    {
        return 32;
    }
    return 64;
}

// Tessellation Factors Phase
struct HS_CONSTANT_DATA_OUTPUT
{
    /* zneg,xneg, zpos, xpos
        vertex pos
    3 2
    1 0
*/
    float edges[4] : SV_TessFactor;
    float inside[2] : SV_InsideTessFactor;
};
// Ran once per patch
HS_CONSTANT_DATA_OUTPUT HSConstantFunction(InputPatch<HS_INPUT_PATCH, 4> patch, uint patchID : SV_PrimitiveID)
{
    const float mult = 0.1;
    HS_CONSTANT_DATA_OUTPUT output;
    const float def = 16.f;
    output.edges[0] = def;
    output.edges[1] = def;
    output.edges[2] = def;
    output.edges[3] = def;

    //output.edges[2] = dostuff(patch[1].Position, patch[0].Position);
    //output.edges[3] = dostuff(patch[1].Position, patch[3].Position);
    //output.edges[0] = dostuff(patch[2].Position, patch[0].Position);
    //output.edges[1] = dostuff(patch[2].Position, patch[3].Position);
    
    output.edges[0] *= TessellationFactor.r;
    output.edges[1] *= TessellationFactor.g;
    output.edges[2] *= TessellationFactor.b;
    output.edges[3] *= TessellationFactor.a;

    output.inside[1] = (output.edges[1] + output.edges[2]) / 2;
    output.inside[0] = (output.edges[0] + output.edges[3]) / 2;

    
    return output;
}
