#include "constants.hlsli"
#define PI		3.1415926535897932
#define TWO_PI	6.2831853071795864
struct cameraConstants
{
    float4x4 vMatrix;
    float4x4 pMatrix;
    float4x4 vpMatrix;
    float4x4 INVvMatrix;
    float4x4 INVpMatrix;
    float4x4 INVvpMatrix;
    float3 cameraPos;
};
struct DebugValues
{
    float4 pixelMult;
    float4 blendDistances;
    uint4 swizzleOrder;
    float4 foamInfo;
    float3 patchSizes;
    // 0: use displacement
    // 2: use foam
    // 3: use channel1
    // 4: use channel2
    // 5: use channel3
    // 6: display texture instead of shader
    // 7: show normal
    // 8: DS show albedo
    // 9: DS show normal
    // 10: DS show neg normal
    // 11: DS show pos
    // 12: DS show materialVal
    // 13: DS show depth
    // Bits [24,31] are reversed for debug reasons
    uint flags;
    float EnvMapMult;
    int maxConeStep;

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
    float4 displacementLambda;
    float patchSize;
    float foamExponentialDecay;
    float foamMinValue;
    float foamBias;
    float foamMult;
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

float3 SampleSkyboxCommon(float3 dir)
{
    float3 negColor = float3(0.0, 0.0, 0.0);
    float3 brightBlueSkyColor = float3(34, 87, 112) / 255;
    float3 darkSkyColor =
    float3(17, 45, 92) / 255;
    float isneg = step(dir.y, 0);

    float3 inp = lerp(brightBlueSkyColor, darkSkyColor, pow(dir.y, 2));
    
    return dir.y < 0 ? negColor : inp;
    
}


// Return 1 if the distance is lower than prevMax
// Return 0 if the distance is higher than currMax
// Return a number in [0,1] as a linear interpolation if between the two values
float GetMultiplier(float prevMax, float currMax, float distance)
{

    return clamp(1 - (distance - prevMax) / (currMax - prevMax), 0, 1);
}


float SchlickFresnel(float f0, float NdotV)
{
    float x = 1.0 - NdotV;
    float x2 = x * x;
    float x5 = x2 * x2 * x;
    return f0 + (1 - f0) * x5;
}


float SmithMaskingBeckmann(float3 H, float3 S, float roughness)
{
    float hdots = max(0.001f, max(dot(H, S), 0));
    float a = hdots / (roughness * sqrt(1 - hdots * hdots));
    float a2 = a * a;

    return a < 1.6f ? (1.0f - 1.259f * a + 0.396f * a2) / (3.535f * a + 2.181 * a2) : 1e-4f;
}

float Beckmann(float ndoth, float roughness)
{
    float exp_arg = (ndoth * ndoth - 1) / (roughness * roughness * ndoth * ndoth);

    return exp(exp_arg) / (PI * roughness * roughness * ndoth * ndoth * ndoth * ndoth);
}


float D_GGX(float NdotH, float roughness)
{
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float denom = NdotH * NdotH * (alpha2 - 1.0) + 1.0;
    return alpha2 / (PI * denom * denom);
}

float G_Smith(float NdotL, float NdotV, float roughness)
{
    float alpha = roughness * roughness;
    float k = alpha * 0.5;
    float G_V = NdotV / (NdotV * (1.0 - k) + k);
    float G_L = NdotL / (NdotL * (1.0 - k) + k);
    return G_V * G_L;
}
double3 hetdiv(double4 v)
{
    return v.xyz / v.w;
}
float3 hetdiv(float4 v)
{
    return v.xyz / v.w;
}


float2 OctWrap(float2 v)
{
    return (1.0 - abs(v.yx)) * (v.xy >= 0.0 ? 1.0 : -1.0);
}
 
float2 OctahedronNormalEncode(float3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    n.xy = n.z >= 0.0 ? n.xy : OctWrap(n.xy);
    n.xy = n.xy * 0.5 + 0.5;
    return n.xy;
}
 
float3 OctahedronNormalDecode(float2 f)
{
    f = f * 2.0 - 1.0;
 
    // https://twitter.com/Stubbesaurus/status/937994790553227264
    float3 n = float3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = saturate(-n.z);
    n.xy += n.xy >= 0.0 ? -t : t;
    return normalize(n);
}


struct SingleLightData
{
    float4 lightPos;
    float4 lightColor;
    float4 AmbientColor;
};
struct SceneLights
{
    SingleLightData lights[MAX_LIGHT_COUNT];
    int lightCount;
};

#define byte_sized_object uint8_t


struct ConeMarchResult
{
    float2 uv;
    float height;
    uint flags; // 0 = hit,
                // 1 = miss
                // 2 = went outside
};

#define BIT(x) (1 << x)
#define CONSERVATIVE_STEP true

// inverts a 3x3 matrix
// input: the 3 columns of the matrix
float3x3 invMat(float3 a, float3 b, float3 c)
{
    float3 r1 = cross(b, c);
    float3 r2 = cross(c, a);
    float3 r3 = cross(a, b);
    float invDet = 1 / dot(r3, c);
    return invDet * float3x3(r1, r2, r3);
}
