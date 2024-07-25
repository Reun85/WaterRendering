    // Texture and RWTexture declarations
Texture2D<float4> displacement : register(t0);
RWTexture2D<float4> gradients : register(u0);

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{

    // Define constants
    uint2 DISP_MAP_SIZE = 1024; // Define as per your requirement

    float PATCH_SIZE = 100.f;
    float TILE_SIZE_X2 = PATCH_SIZE * 2.0f / DISP_MAP_SIZE; // Define as per your requirement
    float INV_TILE_SIZE = DISP_MAP_SIZE / PATCH_SIZE; // Define as per your requirement

    // Calculate the location
    int2 loc = int2(dispatchThreadID.xy);

    // Calculate neighboring locations
    int2 left = (loc - int2(1, 0)) & (DISP_MAP_SIZE - 1);
    int2 right = (loc + int2(1, 0)) & (DISP_MAP_SIZE - 1);
    int2 bottom = (loc - int2(0, 1)) & (DISP_MAP_SIZE - 1);
    int2 top = (loc + int2(0, 1)) & (DISP_MAP_SIZE - 1);

    // Load displacement values
    float3 disp_left = displacement[left].xyz;
    float3 disp_right = displacement[right].xyz;
    float3 disp_bottom = displacement[bottom].xyz;
    float3 disp_top = displacement[top].xyz;

    // Calculate gradient
    float2 gradient = float2(disp_left.y - disp_right.y, disp_bottom.y - disp_top.y);

    // Calculate Jacobian
    float2 dDx = (disp_right.xz - disp_left.xz) * INV_TILE_SIZE;
    float2 dDy = (disp_top.xz - disp_bottom.xz) * INV_TILE_SIZE;

    float J = (1.0 + dDx.x) * (1.0 + dDy.y) - dDx.y * dDy.x;

    // Store the result in the gradients texture
    gradients[loc] = float4(gradient, TILE_SIZE_X2, J);
}
