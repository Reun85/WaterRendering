Texture2D<float4> displacement : register(t0);
RWTexture2D<float4> gradients : register(u0);

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{

    uint DISP_MAP_SIZE = 1024;

    float PATCH_SIZE = 20.f;
    float TILE_SIZE_X2 = PATCH_SIZE * 2.0f / float(DISP_MAP_SIZE);
    float INV_TILE_SIZE = DISP_MAP_SIZE / PATCH_SIZE;

    int2 loc = int2(dispatchThreadID.xy);

    int2 left = (loc - int2(1, 0)) & (DISP_MAP_SIZE - 1);
    int2 right = (loc + int2(1, 0)) & (DISP_MAP_SIZE - 1);
    int2 bottom = (loc - int2(0, 1)) & (DISP_MAP_SIZE - 1);
    int2 top = (loc + int2(0, 1)) & (DISP_MAP_SIZE - 1);

    float3 disp_left = displacement[left].xyz;
    float3 disp_right = displacement[right].xyz;
    float3 disp_bottom = displacement[bottom].xyz;
    float3 disp_top = displacement[top].xyz;

    float2 gradient = float2(disp_left.y - disp_right.y, disp_bottom.y - disp_top.y);

    float2 dDx = (disp_right.xz - disp_left.xz) * INV_TILE_SIZE;
    float2 dDy = (disp_top.xz - disp_bottom.xz) * INV_TILE_SIZE;

    float J = (1.0 + dDx.x) * (1.0 + dDy.y) - dDx.y * dDy.x;

    gradients[loc] = float4(gradient.y, TILE_SIZE_X2, gradient.x, J);
}
