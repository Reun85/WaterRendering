#include "common.hlsli"
Texture2D<float4> _heightmap1 : register(t0);
Texture2D<float4> _heightmap2 : register(t1);
Texture2D<float4> _heightmap3 : register(t2);
Texture2D<float4> gradients1 : register(t3);
Texture2D<float4> gradients2 : register(t4);
Texture2D<float4> gradients3 : register(t5);
SamplerState _sampler : register(s0);

cbuffer CameraBuffer : register(b0)
{
    cameraConstants camConstants;
}

cbuffer DomainBuffer : register(b9)
{
    DebugValues debugValues;
};

struct DS_OUTPUT
{
    float4 Position : SV_POSITION;
    float3 localPos : POSITION;
    float2 TexCoord : PLANECOORD;
    float4 grad : GRADIENTS;
};

struct HS_OUTPUT_PATCH
{
    float3 localPos : POSITION;
    float2 TexCoord : PLANECOORD;
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

    const float2 planeCoord = (patch[0].TexCoord * (1.0f - UV.x) + patch[1].TexCoord * UV.x) * (1.0f - UV.y) +
                      (patch[2].TexCoord * (1.0f - UV.x) + patch[3].TexCoord * UV.x) * UV.y;



    const float3 viewDir = camConstants.cameraPos - localPos;
    const float viewDistanceSqr = dot(viewDir, viewDir);
    const float HighestMax = sqr(debugValues.blendDistances.r);
    const float MediumMax = sqr(debugValues.blendDistances.g);
    const float LowestMax = sqr(debugValues.blendDistances.b);

    float HighestMult = GetMultiplier(0, HighestMax, viewDistanceSqr);
    float MediumMult = GetMultiplier(HighestMax, MediumMax, viewDistanceSqr);
    // Multiplied by (1-HighestMult), essentially turning it off if the highestresolution waves are used, 
    // and will gradually merge between them when the highests are no longer used and the lowest are used
    float LowestMult = GetMultiplier(MediumMax, LowestMax, viewDistanceSqr) * (1 - HighestMult);

    float highestaccountedfor = 0;

    
    float4 grad = float4(0, 0, 0, 0);
    // Use displacement
    if (has_flag(debugValues.flags, 0))
    {
        float4 disp = float4(0, 0, 0, 0);
        if (has_flag(debugValues.flags, 3) && HighestMult > 0)
        {
            highestaccountedfor = HighestMax;
            float2 texCoord = GetTextureCoordFromPlaneCoordAndPatch(planeCoord, debugValues.patchSizes.r);
            disp += _heightmap1.SampleLevel(_sampler, texCoord, 0) * HighestMult;
            grad += gradients1.SampleLevel(_sampler, texCoord, 0) * HighestMult;
        }
        if (has_flag(debugValues.flags, 4) && MediumMult > 0)
        {
            highestaccountedfor = MediumMax;
            float2 texCoord = GetTextureCoordFromPlaneCoordAndPatch(planeCoord, debugValues.patchSizes.g);
            disp += _heightmap2.SampleLevel(_sampler, texCoord, 0) * MediumMult;
            grad += gradients2.SampleLevel(_sampler, texCoord, 0) * MediumMult;
        }
        if (has_flag(debugValues.flags, 5) && LowestMult > 0)
        {
        
            highestaccountedfor = LowestMax;
            float2 texCoord = GetTextureCoordFromPlaneCoordAndPatch(planeCoord, debugValues.patchSizes.b);
            disp += _heightmap3.SampleLevel(_sampler, texCoord, 0) * LowestMult;
            grad += gradients3.SampleLevel(_sampler, texCoord, 0) * LowestMult;
        }
        localPos += disp.xyz;
    }
    else
    {
        grad = float4(0, 1, 0, 0);
    }
    if (viewDistanceSqr > highestaccountedfor)
    {
        grad = float4(0, 1, 0, 0);
    }
    
    
    
    float4 position = mul(float4(localPos, 1), camConstants.vpMatrix);
    output.Position = position;
    //position = float4(hetdiv(position), 1);
    //localPos = hetdiv(mul(position, camConstants.INVvpMatrix)).xyz;
    output.TexCoord = planeCoord;
    output.localPos = localPos;
    output.grad = grad;
    
    
    return output;
}
