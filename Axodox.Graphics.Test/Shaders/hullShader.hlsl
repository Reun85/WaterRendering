
cbuffer HullBuffer : register(b1)
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
    float3 localPos : POSITION;
    float2 TexCoord : TEXCOORD;
};

struct HS_OUTPUT_PATCH
{
    // The domain shader recalculates the screen position
    float3 localPos : POSITION;
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

    output.TexCoord = patch[i].TexCoord;
    output.localPos = patch[i].localPos;
    
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



inline float toNearestPowerOfTwo(float a)
{
    const float u = a;
    
    if (u <= 1)
    {
        return 1;
    }
    if (u <= 2)
    {
        return 2;
    }
    if (u <= 4)
    {
        return 4;
    }
    if (u <= 8)
    {
        return 8;
    }
    if (u <= 16)
    {
        return 16;
    }
    if (u <= 32)
    {
        return 32;
    }
    return 64;
}
//float dlodSphere(float4 p0, float4 p1, float4 t0, float4 t1)
//{
//    float g_tessellatedTriWidth = 10.0;

//    float4 samp = texture(TexTerrainHeight, t0);
//    p0.y = samp[0] * TerrainHeightOffset;
//    samp = texture(TexTerrainHeight, t1);
//    p1.y = samp[0] * TerrainHeightOffset;

//    float4 center = 0.5 * (p0 + p1);
//    float4 view0 = mvMatrix * center;
//    float4 view1 = view0;
//    view1.x += distance(p0, p1);

//    float4 clip0 = pMatrix * view0;
//    float4 clip1 = pMatrix * view1;

//    clip0 /= clip0.w;
//    clip1 /= clip1.w;

//	//clip0.xy *= Viewport;
//	//clip1.xy *= Viewport;

//    float2 screen0 = ((clip0.xy + 1.0) / 2.0) * Viewport;
//    float2 screen1 = ((clip1.xy + 1.0) / 2.0) * Viewport;
//    float d = distance(screen0, screen1);

//	// g_tessellatedTriWidth is desired pixels per tri edge
//    float t = clamp(d / g_tessellatedTriWidth, 0, 64);

//    return toNearestPowerOfTwo(t);

//}


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
    
    output.edges[0] *= TessellationFactor.g;
    output.edges[1] *= TessellationFactor.b;
    output.edges[2] *= TessellationFactor.a;
    output.edges[3] *= TessellationFactor.r;

    output.inside[1] = (output.edges[1] + output.edges[2]) / 2;
    output.inside[0] = (output.edges[0] + output.edges[3]) / 2;

    
    return output;
}
