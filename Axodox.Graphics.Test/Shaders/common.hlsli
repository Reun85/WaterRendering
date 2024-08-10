
#define NUM_INSTANCES (1 << 11)
#define DISP_MAP_LOG2 10
#define DISP_MAP_SIZE (1<<DISP_MAP_LOG2)
#define PATCH_SIZE 40.f

#define DEFAULT_TESSELATION 16.f
#define DISPLACEMENT_LAMBDA 1.3f
struct cameraConstants
{
    float4x4 vMatrix;
    float4x4 pMatrix;
    float4x4 vpMatrix;
    float3 cameraPos;
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
