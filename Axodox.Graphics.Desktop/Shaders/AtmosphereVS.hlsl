
#include "common.hlsli"

cbuffer camBuffer : register(b0)
{
    cameraConstants camConstants;
};

struct VS_INPUT
{
    float3 Pos : POSITION;
    float3 normal : NORMAL;
    float2 tex : TEXCOORD;
};
struct VS_OUTPUT
{
    float4 Pos : SV_POSITION;
    float3 TexCoord : TEXCOORD0;
};


VS_OUTPUT main(VS_INPUT input)
{

    VS_OUTPUT output;
    
    float3 pos = input.Pos;
    pos += camConstants.cameraPos;
    output.Pos = mul(float4(pos, 1), camConstants.vpMatrix).xyww;
    output.TexCoord = input.Pos;
    
    return output;
}