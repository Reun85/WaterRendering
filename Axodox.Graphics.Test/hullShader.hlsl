cbuffer HullBuffer : register(b0)
{
    float4x4 Transformation;
    // zneg,xneg, zpos, xpos
    float4 TessellationFactor;
};

// --------------------------------------
// Hull Shader
// --------------------------------------

// Output
struct HS_CONTROL_POINT
{
    float3 Position : POSITION;
    float2 TexCoord : TEXCOORD;
};

// Input
struct HS_INPUT_PATCH
{
    float3 Position : POSITION;
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
    output.Position = mul(float4(
    patch[i].
Position, 1), Transformation);

    
    output.TexCoord = patch[i].
TexCoord;
    return
output;
}

// Tessellation Factors Phase
struct HS_CONSTANT_DATA_OUTPUT
{
    // zneg,xneg, zpos, xpos
    float edges[4] : SV_TessFactor;
    float inside[2] : SV_InsideTessFactor;
};

// Ran once per patch
HS_CONSTANT_DATA_OUTPUT HSConstantFunction(InputPatch<HS_INPUT_PATCH, 4> patch, uint patchID : SV_PrimitiveID)
{
    HS_CONSTANT_DATA_OUTPUT output;
    output.edges[0] = 32 * TessellationFactor.r;
    output.edges[1] = 32 * TessellationFactor.g;
    output.edges[2] = 32 * TessellationFactor.b;
    output.edges[3] = 32 * TessellationFactor.a;
    output.inside[0] = 32;
    output.inside[1] = 32;
    //output.edges[0] = 1;
    //output.edges[1] = 1;
    //output.edges[2] = 1;
    //output.edges[3] = 1;
    //output.inside[0] = 0;
    //output.inside[1] = 0;
    
    return output;
}
