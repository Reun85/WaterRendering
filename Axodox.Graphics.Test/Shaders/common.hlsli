
#include "constants.hlsli"
struct cameraConstants
{
    float4x4 vMatrix;
    float4x4 pMatrix;
    float4x4 vpMatrix;
    float3 cameraPos;
};
struct DebugValues
{
    float4 pixelMult;
    float4 blendDistances;
    uint4 swizzleOrder;
    float3 patchSizes;
    // 0: use displacement
    // 2: use foam
    // 3: use channel1
    // 4: use channel2
    // 5: use channel3
    // 6: display texture instead of shader
    // 7: show normal
    // Bits [24,31] are reversed for debug reasons
    uint flags;
    float3 displacementMult;

};


bool has_flag(uint flags, uint flagIndex)
{
    return (flags & (1u << flagIndex)) != 0;
}

float4 GetRemainingFraction(float4 value, float fraction)
{
    return fmod(value, fraction) / fraction;
}
float3 GetRemainingFraction(float3 value, float fraction)
{
    return fmod(value, fraction) / fraction;
}
float2 GetRemainingFraction(float2 value, float fraction)
{
    return fmod(value, fraction) / fraction;
}
float GetRemainingFraction(float value, float fraction)
{
    return fmod(value, fraction) / fraction;
}


float sqr(float x)
{
    return x * x;
}

float DotClamped(float3 x, float3 y)
{
    return max(0, dot(x, y));
}
float DotClamped(float2 x, float2 y)
{
    return max(0, dot(x, y));
}
float DotClamped(float x, float y)
{
    return max(0, dot(x, y));
}


float2 GetTextureCoordFromPlaneCoordAndPatch(float2 planeCoord, float patchSize)
{
    return GetRemainingFraction(planeCoord, patchSize) + 0.5.xx;
}

struct TimeConstants
{
    float deltaTime;
    float timeSinceLaunch;
};


struct ComputeConstants
{
    float patchSize;
    float displacementLambda;
    float foamExponentialDecay;
};

inline float2 ComplexMul(float2 a, float2 b)
{
    return float2(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x);
}

float GetComponentByIndex(float4 vec, uint index)
{

    if (index == 0)
        return vec.x;
    if (index == 1)
        return vec.y;
    if (index == 2)
        return vec.z;
    if (index == 3)
        return vec.w;
    return 0.0; // Default case, should not occur if indices are valid
}

float4 Swizzle(float4 vec, uint4 order)
{
    return float4(
        GetComponentByIndex(vec, order.x),
        GetComponentByIndex(vec, order.y),
        GetComponentByIndex(vec, order.z),
        GetComponentByIndex(vec, order.w)
    );
}
