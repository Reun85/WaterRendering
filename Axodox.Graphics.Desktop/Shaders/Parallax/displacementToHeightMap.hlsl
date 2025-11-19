#include "../common.hlsli"
Texture2D<float4> displacement : register(t0);
Texture2D<float4> gradients : register(t1);
RWTexture2D<float> heightMap : register(u0);
RWTexture2D<float4> outGradient : register(u1);

cbuffer Test : register(b9)
{
    ComputeConstants constants;
}


#define N DISP_MAP_SIZE
#define LOG2_M 4
#define M (1<<LOG2_M)

// The + are overhangs
[numthreads(M, M, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint3 LTid : SV_GroupThreadID, uint3 GTid : SV_GroupID)
{
   
    int2 groupID = GTid.xy;
    int2 threadID = LTid.xy;

    

    heightMap[DTid.xy] = 0;
    outGradient[DTid.xy] = float4(0, 0, 0, 0);
    
    GroupMemoryBarrierWithGroupSync();

    float3 data = displacement.Load(int3(DTid.xy, 0)).xyz;
    float h = data.y;
    float2 disp = data.xz;
    // Approximate new textureCoordinates for the heightmap.
    // The texture wraps around every in constants.patchSize coordinates

    disp /= constants.patchSize;
    // wrap to 0,1
    disp = frac(disp);

    disp *= constants.patchSize;
    // get the four neighboring texels coordinates uint2s! and coefficients
    

    int2 baseTexCoord = int2(disp);

    float2 fracPart = frac(disp);

    int2 tex00 = baseTexCoord;
    int2 tex10 = baseTexCoord + int2(1, 0);
    int2 tex01 = baseTexCoord + int2(0, 1);
    int2 tex11 = baseTexCoord + int2(1, 1);

    tex00 = tex00 % constants.patchSize;
    tex10 = tex10 % constants.patchSize;
    tex01 = tex01 % constants.patchSize;
    tex11 = tex11 % constants.patchSize;
    
    float w00 = (1.0 - fracPart.x) * (1.0 - fracPart.y);
    float w10 = fracPart.x * (1.0 - fracPart.y);
    float w01 = (1.0 - fracPart.x) * fracPart.y;
    float w11 = fracPart.x * fracPart.y;

    
    // InterlockedAdd is not defined for floats :c
    heightMap[tex00] += h * w00;
    heightMap[tex10] += h * w10;
    heightMap[tex01] += h * w01;
    heightMap[tex11] += h * w11;

    float4 gradientData = gradients.Load(int3(DTid.xy, 0));

    outGradient[tex00] += gradientData * w00;
    outGradient[tex10] += gradientData * w10;
    outGradient[tex01] += gradientData * w01;
    outGradient[tex11] += gradientData * w11;
}
