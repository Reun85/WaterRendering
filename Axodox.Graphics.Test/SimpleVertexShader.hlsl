
cbuffer VertexBuffer : register(b0)
{
    float4x4 Transformation;
};
struct input_t
{
    float3 Position : POSITION;
    float2 Texture : TEXCOORD;
};
struct output_t
{
    float4 Position : SV_POSITION;
    float2 Texture : TEXCOORD;
};





output_t main(input_t input)
{
    output_t output;
    output.Position = mul(float4(input.Position, 1), Transformation);
    output.Texture = input.Texture;
    return output;
}