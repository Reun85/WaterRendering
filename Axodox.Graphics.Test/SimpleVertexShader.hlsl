

Texture2D<float> _heightmap : register(t0);
SamplerState _sampler : register(s0);

cbuffer Constants : register(b0)
{
    float4x4 Transformation;
    float4x4 WorldIT;
    float4x4 TextureTransform;
    uint2 subdivisions;
};

struct input_t
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 Texture : TEXCOORD;
    uint vertexId : SV_VertexID;
};

struct output_t
{
    float4 Screen : SV_POSITION;
    float2 Texture : TEXCOORD;
    float3 normal : NORMAL;
};




output_t main(input_t input)
{
    output_t output;
    

    
    const uint2 subs = subdivisions;
    const float2 step = float2(1.0f / float(subs.x - 1), 1.0f / float(subs.y - 1));

    
    
    const uint x = input.vertexId % subs.x;
    const uint y = input.vertexId / subs.y;

    
    
    float2 res =
    float2(x, subs.y - y) * step;

    
    
    output.Texture = mul(TextureTransform, float4(res.x, 0, res.y, 1)).xz;
    float4 text;
    text = _heightmap.SampleLevel(_sampler, output.Texture, 0);
    output.Screen = mul(float4(input.Position, 1), Transformation);
    output.Screen += float4(0.0f, text.r, 0.0f, 0.0f);
    // This would work, but the heightmap does not contain the normals on the gba channels
    output.normal = mul(float4(text.gba, 1), WorldIT).xyz;
    return output;
}