#include "common.hlsli"

cbuffer CameraBuffer : register(b0)
{
    cameraConstants camConstants;
};
cbuffer ModelBuffer : register(b1)
{
    float4x4 mMatrix;
};
cbuffer VertexBuffer : register(b2)
{
    float4x4 Transformation;
    float2 PlaneBottomLeft;
    float2 PlaneTopRight;
};
struct input_t
{
    float3 Position : POSITION;
    uint instanceID : SV_InstanceID;
};
struct output_t
{
    float4 Position : SV_POSITION;
    float3 localPos : POSITION;
    float2 TexCoord : TEXCOORD;
};






output_t main(input_t input)
{
    output_t output;
    float4 position
     = mul(float4(input.Position, 1), mMatrix);
    position = mul(position, Transformation);
    //Maybe move this to the domain shader later?
    float2 texCoord = (position.xz - PlaneBottomLeft) / (PlaneTopRight - PlaneBottomLeft);
    // If the quad is rotated
    texCoord = ((PlaneBottomLeft.x < PlaneTopRight.x && PlaneBottomLeft.y > PlaneTopRight.y) || (PlaneBottomLeft.x > PlaneTopRight.x && PlaneBottomLeft.y < PlaneTopRight.y) ? texCoord.xy : texCoord.yx);
    // For normal images
    

    float4 screenPosition = mul(position, camConstants.vpMatrix);
    output.Position = screenPosition;
    output.localPos = position.xyz;
    output.TexCoord = texCoord;
    return output;
}