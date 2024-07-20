RWTexture2D<float> outputTexture : register(u0);

cbuffer buff : register(b0)
{
    float time;
};
[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint2 dims = (1024, 1024);
    float v = sin(time + (float) DTid.x / 8);
    outputTexture[DTid.xy] = v / 2;
}