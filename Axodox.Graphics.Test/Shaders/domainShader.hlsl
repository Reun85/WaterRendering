#include "common.hlsli"
Texture2D<float4> _heightmap : register(t0);
Texture2D<float4> _gradients : register(t1);
SamplerState _sampler : register(s0);

cbuffer CameraBuffer : register(b0)
{
    cameraConstants camConstants;
}

cbuffer DomainBuffer : register(b1)
{
    int useDisplacement;
};

struct DS_OUTPUT
{
    float4 Position : SV_POSITION;
    float3 localPos : POSITION;
    float2 TexCoord : TEXCOORD;
    float4 Normal : Variable;
};

struct HS_OUTPUT_PATCH
{
    float3 localPos : POSITION;
    float2 TexCoord : TEXCOORD;
};


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
    const bool apply_disp = true;
    DS_OUTPUT output;
    
    // Perform bilinear interpolation of the control points
    float3 localPos = (patch[0].localPos * (1.0f - UV.x) + patch[1].localPos * UV.x) * (1.0f - UV.y) +
                      (patch[2].localPos * (1.0f - UV.x) + patch[3].localPos * UV.x) * UV.y;

    float2 texCoord = (patch[0].TexCoord * (1.0f - UV.x) + patch[1].TexCoord * UV.x) * (1.0f - UV.y) +
                      (patch[2].TexCoord * (1.0f - UV.x) + patch[3].TexCoord * UV.x) * UV.y;

    
    if (useDisplacement == 0)
    {
        float4 disp = _heightmap.SampleLevel(_sampler, texCoord, 0);
        localPos += disp.xyz;
    }
    float4 normal = _gradients.SampleLevel(_sampler, texCoord, 0);
    
    
    
    float4 position = mul(float4(localPos, 1), camConstants.vpMatrix);
    output.Position = position;
    output.TexCoord = texCoord;
    output.Normal = normal;
    output.localPos = localPos;
    
    return output;
}
