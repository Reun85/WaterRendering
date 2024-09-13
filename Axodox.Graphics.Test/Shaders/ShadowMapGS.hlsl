// THIS IS JUST A CONCEPT!
#include "common.hlsli"
cbuffer LightSpaceMatrices : register(b4)
{
    float4x4 lightSpaceMatrices[MAX_SHADOWMAP_MATRICES];
    uint shadowMapMatrixCount;
};


struct input_t
{
    float4 Screen : SV_POSITION;
    float3 localPos : POSITION;
    float2 planeCoord : PLANECOORD;
    float4 grad : GRADIENTS;
};

struct output_t
{
    float4 Position : SV_POSITION;
    uint Layer : SV_RenderTargetArrayIndex;
};

//this should be multiplied by MAX_SHADOWMAP_MATRICES
[maxvertexcount(3 * 3)] // 3 vertices * 3 layers
void main(triangle input_t input[3], inout TriangleStream<output_t> TriStream)
{
    for (uint InvocationID = 0; InvocationID < 3; ++InvocationID)
    {
        for (int i = 0; i < 3; ++i)
        {
            output_t
            output;
            output.Position = mul(lightSpaceMatrices[InvocationID], input[i].Screen);
            output.Layer = InvocationID;
            TriStream.Append(output);
        }
        TriStream.RestartStrip();
    }
}
