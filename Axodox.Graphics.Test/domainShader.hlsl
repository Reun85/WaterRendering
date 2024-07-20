Texture2D _heightmap : register(t0);
SamplerState _sampler : register(s0);

cbuffer DomainBuffer : register(b0)
{
    float4x4 TextureTransform;
    float4x4 WorldIT;
};
// --------------------------------------
// Domain Shader
// --------------------------------------

struct DS_OUTPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD;
    float3 Normal : NORMAL;
};

struct HS_OUTPUT_PATCH
{
    float4 Position : SV_POSITION;
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

    float2 texCoord = (patch[0].TexCoord * (1.0f - UV.x) + patch[1].TexCoord * UV.x) * (1.0f - UV.y) +
                      (patch[2].TexCoord * (1.0f - UV.x) + patch[3].TexCoord * UV.x) * UV.y;
    
    
    texCoord = mul(TextureTransform, float4(texCoord.x, 0, texCoord.y, 1)).xz;
    float4 text = _heightmap.SampleLevel(_sampler, texCoord, 0);
    position += float4(0, text.r, 0, 0) * 1;
    // This would work, but the heightmap does not contain the normals on the gba channels
    float3 normal = mul(float4(text.gba, 1), WorldIT).xyz;
    
    
    
    output.Position = position;
    output.TexCoord = texCoord;
    output.Normal = normal;
    
    return output;
}