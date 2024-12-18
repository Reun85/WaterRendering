#include "common.hlsli"
Texture2D<float4> _heightmap1 : register(t0);
Texture2D<float4> _heightmap2 : register(t1);
Texture2D<float4> _heightmap3 : register(t2);
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

    //float HighestMult = GetMultiplier(0, HighestMax, viewDistanceSqr);
    //float MediumMult = GetMultiplier(HighestMax, MediumMax, viewDistanceSqr);
    //// Multiplied by (1-HighestMult), essentially turning it off if the highestresolution waves are used, 
    //// and will gradually merge between them when the highests are no longer used and the lowest are used
    //float LowestMult = GetMultiplier(MediumMax, LowestMax, viewDistanceSqr);
    float HighestMult = 1;
    float MediumMult = 1;
    float LowestMult = 1;

    float highestaccountedfor = 0;

    
    // Use displacement
    if (has_flag(debugValues.flags, 0))
    {
        float4 disp = float4(0, 0, 0, 0);
        //if (has_flag(debugvalues.flags, 3) && highestmult > 0)
        //{
        //    highestaccountedfor = highestmax;
        //    float2 texcoord = gettexturecoordfromplanecoordandpatch(planecoord, debugvalues.patchsizes.r);
        //    disp += _heightmap1.samplelevel(_sampler, texcoord, 0) * highestmult;
        //    grad += gradients1.samplelevel(_sampler, texcoord, 0);
        //}
        //if (has_flag(debugvalues.flags, 4) && mediummult > 0)
        //{
        //    highestaccountedfor = mediummax;
        //    float2 texcoord = gettexturecoordfromplanecoordandpatch(planecoord, debugvalues.patchsizes.g);
        //    disp += _heightmap2.samplelevel(_sampler, texcoord, 0) * mediummult;
        //    grad += gradients2.samplelevel(_sampler, texcoord, 0);
        //}
        //if (has_flag(debugvalues.flags, 5) && lowestmult > 0)
        //{
        
        //    highestaccountedfor = lowestmax;
        //    float2 texcoord = gettexturecoordfromplanecoordandpatch(planecoord, debugvalues.patchsizes.b);
        //    disp += _heightmap3.samplelevel(_sampler, texcoord, 0) * lowestmult;
        //    grad += gradients3.samplelevel(_sampler, texcoord, 0);
        //}

        if (has_flag(debugValues.flags, 3) && HighestMult > 0)
        {
        
            highestaccountedfor = HighestMax;
            float2 texCoord = GetTextureCoordFromPlaneCoordAndPatch(planeCoord, debugValues.patchSizes.r);
            disp += _heightmap1.SampleLevel(_sampler, texCoord, 0);
        }
        if (has_flag(debugValues.flags, 4) && MediumMult > 0)
        {
            highestaccountedfor = MediumMax;
            float2 texCoord = GetTextureCoordFromPlaneCoordAndPatch(planeCoord, debugValues.patchSizes.g);
            disp += _heightmap2.SampleLevel(_sampler, texCoord, 0);
        }
        if (has_flag(debugValues.flags, 5) && LowestMult > 0)
        {
        
            highestaccountedfor = LowestMax;
            float2 texCoord = GetTextureCoordFromPlaneCoordAndPatch(planeCoord, debugValues.patchSizes.b);
            disp += _heightmap3.SampleLevel(_sampler, texCoord, 0);
        }

        localPos += disp.xyz;
    }
    
    
    
    float4 position = mul(float4(localPos, 1), camConstants.vpMatrix);
    output.Position = position;
    output.TexCoord = planeCoord;
    output.localPos = localPos;
    
    return output;
}
