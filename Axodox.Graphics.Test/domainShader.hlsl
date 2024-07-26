Texture2D<float4> _heightmap : register(t0);
Texture2D<float4> _gradients : register(t1);
SamplerState _sampler : register(s0);

cbuffer DomainBuffer : register(b0)
{
    float4x4 ViewProjTransform;
};
// --------------------------------------
// Domain Shader
// --------------------------------------

struct DS_OUTPUT
{
    float4 Position : SV_POSITION;
    float4 localPos : POSITION;
    float2 TexCoord : TEXCOORD;
    float3 Normal : NORMAL;
};

struct HS_OUTPUT_PATCH
{
    float4 Position : SV_POSITION;
    float4 localPos : POSITION;
    float2 TexCoord : TEXCOORD;
};


// Tessellation Factors Phase
struct HS_CONSTANT_DATA_OUTPUT
{
    float edges[4] : SV_TessFactor;
    float inside[2] : SV_InsideTessFactor;
};


// Ran once per output vertex
[domain("quad")]
DS_OUTPUT main(HS_CONSTANT_DATA_OUTPUT patchConstants,
                               const OutputPatch<HS_OUTPUT_PATCH, 4> patch,
                               float2 UV : SV_DomainLocation)
{
    DS_OUTPUT output;
    
    // Perform bilinear interpolation of the control points
    float4 position = (patch[0].Position * (1.0f - UV.x) + patch[1].Position * UV.x) * (1.0f - UV.y) +
                      (patch[2].Position * (1.0f - UV.x) + patch[3].Position * UV.x) * UV.y;
    float4 localPos = (patch[0].localPos * (1.0f - UV.x) + patch[1].localPos * UV.x) * (1.0f - UV.y) +
                      (patch[2].localPos * (1.0f - UV.x) + patch[3].localPos * UV.x) * UV.y;

    float2 texCoord = (patch[0].TexCoord * (1.0f - UV.x) + patch[1].TexCoord * UV.x) * (1.0f - UV.y) +
                      (patch[2].TexCoord * (1.0f - UV.x) + patch[3].TexCoord * UV.x) * UV.y;

    
    float4 text = _heightmap.SampleLevel(_sampler, texCoord, 0);
    position += mul(ViewProjTransform, float4(text.xyz, 1));

    float3 normal = _gradients.SampleLevel(_sampler, texCoord, 0).xyz;
    
    
    
    output.Position = position;
    output.TexCoord = texCoord;
    output.Normal = normal;
    output.localPos = localPos + float4(text.xyz, 0);
    
    return output;
}
