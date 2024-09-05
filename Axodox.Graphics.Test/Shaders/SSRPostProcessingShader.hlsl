#include "common.hlsli"
Texture2D colorBuffer : register(t0);
Texture2D normalBuffer : register(t1);
Texture2D depthMap : register(t9);

RWTexture2D<unorm float4> _output : register(u0);
SamplerState _sampler : register(s0);

cbuffer Constants : register(b0)
{
    cameraConstants cam;
}
bool rayIsOutofScreen(float2 ray)
{
    return (ray.x > 1 || ray.y > 1 || ray.x < 0 || ray.y < 0);
}
float4 TraceRay(float3 rayPos, float3 dir, int iterationCount)
{
    float sampleDepth;
    float4 hitColor = float4(0, 0, 0, 0);
    bool hit = false;

    for (int i = 0; i < iterationCount; i++)
    {
        rayPos += dir;
        if (rayIsOutofScreen(rayPos.xy))
        {
            break;
        }

        sampleDepth = depthMap.SampleLevel(_sampler, rayPos.xy, 0).r;

        
        float depthDif = rayPos.z - sampleDepth;
        if (depthDif >= 0 && depthDif < 0.00001)
        { //we have a hit
            hit = true;
            hitColor = float4(colorBuffer.SampleLevel(_sampler, rayPos.xy, 0).rgb, 1);
            break;

        }
    }
    return hitColor;
}

[numthreads(16, 16, 1)]
void main(uint3 index : SV_DispatchThreadID)
{
    uint2 size;
    colorBuffer.GetDimensions(size.x, size.y);
  
    float2 uv = (index.xy + 0.5) / size;
    float4 color = colorBuffer.SampleLevel(_sampler, uv, 0);
  

    float maxRayDistance = 200.0f;

    float4 reflectionColor;
    
	//View Space ray calculation
    float3 pixelPositionTexture;
    pixelPositionTexture.xy = uv;
    float3 normalView = mul(float4(normalBuffer.SampleLevel(_sampler, pixelPositionTexture.xy, 0).rgb, 1), transpose
    (cam.vMatrix)).rgb;
    float pixelDepth = depthMap.SampleLevel(_sampler, pixelPositionTexture.xy, 0).r; // 0< <1
    pixelPositionTexture.z = pixelDepth;
    float4 positionView = mul(float4(pixelPositionTexture * 2 - float3(1, 1, 1), 1), cam.INVpMatrix);
    positionView /= positionView.w;
    float3 reflectionView = normalize(reflect(positionView.xyz, normalView));
    if (reflectionView.z > 0)
    {
        reflectionColor = float4(0, 0, 0, 1);
        return;
    }
    float3 rayEndPositionView = positionView.xyz + reflectionView * maxRayDistance;


	//Texture Space ray calculation
    float4 rayEndPositionTexture = mul(float4(rayEndPositionView, 1), cam.pMatrix);
    rayEndPositionTexture /= rayEndPositionTexture.w;
    rayEndPositionTexture.xyz = (rayEndPositionTexture.xyz + float3(1, 1, 1)) / 2.0f;
    float3 rayDirectionTexture = rayEndPositionTexture.xyz - pixelPositionTexture;

    int2 screenSpaceStartPosition = int2(
    pixelPositionTexture.x * size.x, pixelPositionTexture.y * size.y);
    int2 screenSpaceEndPosition = int2(rayEndPositionTexture.x * size.x, rayEndPositionTexture.y * size.y);
    int2 screenSpaceDistance = screenSpaceEndPosition - screenSpaceStartPosition;
    int screenSpaceMaxDistance = max(abs(screenSpaceDistance.x), abs(screenSpaceDistance.y)) / 2;
    rayDirectionTexture /= max(screenSpaceMaxDistance, 0.001f);


	//trace the ray
    reflectionColor = TraceRay(pixelPositionTexture, rayDirectionTexture, screenSpaceMaxDistance);

    reflectionColor.xyz *= SchlickFresnel(0.02, max(0, dot(normalView, float3(0, 0, -1))));
    
    _output[index.xy] = (color + reflectionColor);
}