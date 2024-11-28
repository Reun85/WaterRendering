
#include "../common.hlsli"

Texture2D<float2> heightMap1 : register(t6);
Texture2D<float2> heightMap2 : register(t7);
Texture2D<float2> heightMap3 : register(t8);

SamplerState _sampler : register(s0);
cbuffer CameraBuffer : register(b0)
{
    cameraConstants camConstants;
};

cbuffer ModelBuffer : register(b1)
{
    float4x4 mMatrix;
    float4x4 mINVMatrix;
    float3 center;
    float PrismHeight;
};

struct InstanceData
{
    float2 scaling;
    float2 offset;
};

cbuffer VertexBuffer : register(b2)
{
    InstanceData instances[NUM_INSTANCES];
};


struct output_t
{
    float3 localPos : POSITION;
    uint instanceID : SV_InstanceID;
    float4 screenPos : SV_Position;
};


struct input_t
{
    uint instanceID : SV_InstanceID;
    float3 localPos : Position;
};


cbuffer DebugBuffer : register(b9)
{
    DebugValues debugValues;
}

float2 readtext(in float2 scaling, in float2 offset, in float patchSize, Texture2D<float2> heightMap)
{
    float2 uv = GetTextureCoordFromPlaneCoordAndPatch(offset, patchSize);
    // each texel covers this much area.
    float tileSize = patchSize / DISP_MAP_SIZE;
    int i;
    for (i = 0; i < 4 && all(tileSize.xx > scaling); ++i)
    {
        tileSize *= 2.f;
    }
    int lod = i;
    float2 hoffset = offset / 2;
    float2 huv = GetTextureCoordFromPlaneCoordAndPatch(hoffset, patchSize);
    float2 res1 = heightMap.SampleLevel(_sampler, uv + float2(huv.x, huv.y), lod);
    float2 res2 = heightMap.SampleLevel(_sampler, uv + float2(-huv.x, huv.y), lod);
    float2 res3 = heightMap.SampleLevel(_sampler, uv + float2(huv.x, -huv.y), lod);
    float2 res4 = heightMap.SampleLevel(_sampler, uv + float2(-huv.x, -huv.y), lod);
    float2 res =
    (
    min(min(res1.x, res2.x), min(res3.x, res4.x)),
    max(max(res1.x, res2.x), max(res3.x, res4.x))
    );
    
    return res;

}

float2 readMinMax(in float2 scaling, in float2 offset)
{
    float2 mima = float2(0, 0);
    mima += readtext(scaling, offset, debugValues.patchSizes.r, heightMap1);
    mima += readtext(scaling, offset, debugValues.patchSizes.g, heightMap2);
    mima += readtext(scaling, offset, debugValues.patchSizes.b, heightMap3);

    return mima;
}

output_t main(input_t input)
{
    const float eps = 0.1;
 
    uint instanceID = input.instanceID;
    output_t output;

    const float2 scaling = instances[instanceID].scaling;
    const float2 offset = instances[instanceID].offset;

    float3 localPos = float3(0, 0, 0);
    localPos.xz = input.localPos.xz * scaling + offset;
    float height = PrismHeight;

        
    float2 mima = readMinMax(scaling, offset);
    localPos.y = input.localPos.y > eps ? mima.y : mima.x;
    

    output.localPos = localPos + center;
    output.screenPos = mul(float4(output.localPos, 1), camConstants.vpMatrix);
    output.instanceID = instanceID;

    return output;
}
