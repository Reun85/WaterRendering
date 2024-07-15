
struct input_t
{
    float3 Position : POSITION;
    float2 Texture : TEXCOORD;
};

typedef input_t output_t;




output_t main(input_t input)
{
    return input;
}