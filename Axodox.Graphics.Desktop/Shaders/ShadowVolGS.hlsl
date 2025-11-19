#include "common.hlsli"
cbuffer Constants : register(b0)
{
    float4 l_pos; // Light position (eye space)
    int robust; // Robust generation needed?
    int zpass; // Is it safe to do z-pass?
};

cbuffer CameraConstants : register(b1)
{
    cameraConstants cam;
}

struct VSInput
{
    float4 pos : SV_POSITION;
    float3 localPos : POSITION;
    float2 TexCoord : PLANECOORD;
    float4 grad : GRADIENTS;
};


struct GSOutput
{
    float4 pos : SV_Position;
};

[maxvertexcount(18)]
void main(triangle VSInput input[3], inout TriangleStream<GSOutput> stream)
{
    float3 ns[3];
    float3 d[3];
    float4 v[4];
    float4 or_pos[3] = { input[0].pos, input[1].pos, input[2].pos };

    // Compute normal at each vertex
    //ns[0] = cross(input[1].pos.xyz - input[0].pos.xyz, input[2].pos.xyz - input[0].pos.xyz);
    //ns[1] = cross(input[2].pos.xyz - input[1].pos.xyz, input[0].pos.xyz - input[1].pos.xyz);
    //ns[2] = cross(input[0].pos.xyz - input[2].pos.xyz, input[1].pos.xyz - input[2].pos.xyz);

    ns[0] = normalize(input[0].grad.xyz);
    ns[1] = normalize(input[1].grad.xyz);
    ns[2] = normalize(input[2].grad.xyz);

    // Compute direction from vertices to light
    d[0] = l_pos.xyz - l_pos.w * input[0].pos.xyz;
    d[1] = l_pos.xyz - l_pos.w * input[1].pos.xyz;
    d[2] = l_pos.xyz - l_pos.w * input[2].pos.xyz;

    // Check if the main triangle faces the light
    bool faces_light = true;
    if (!(dot(ns[0], d[0]) > 0 || dot(ns[1], d[1]) > 0 || dot(ns[2], d[2]) > 0))
    {
        if (robust == 0)
            return;
        or_pos[1] = input[2].pos;
        or_pos[2] = input[1].pos;
        faces_light = false;
    }

    // Render caps. This is only needed for z-fail.
    if (zpass == 0)
    {
        // Near cap: simply render triangle
        GSOutput output;
        output.pos = or_pos[0];
        stream.Append(output);
        output.pos = or_pos[1];
        stream.Append(output);
        output.pos = or_pos[2];
        stream.Append(output);
        stream.RestartStrip();

        // Far cap: extrude positions to infinity
        v[0] = float4(l_pos.w * or_pos[0].xyz - l_pos.xyz, 0);
        v[1] = float4(l_pos.w * or_pos[1].xyz - l_pos.xyz, 0);
        v[2] = float4(l_pos.w * or_pos[2].xyz - l_pos.xyz, 0);

        // CHANGE:  Check that NDC is correct
        output.pos = mul(v[0], cam.vpMatrix);
        stream.Append(output);
        output.pos = mul(v[1], cam.vpMatrix);
        stream.Append(output);
        output.pos = mul(v[2], cam.vpMatrix);
        stream.Append(output);
        stream.RestartStrip();
    }

    // Loop over all edges and extrude if needed
    for (int i = 0; i < 3; i++)
    {
        int v0 = i;
        int v1 = (i + 1) % 3;
        
        // Compute normals at vertices
        ns[0] = cross(input[v1].pos.xyz - input[v0].pos.xyz, input[(v0 + 2) % 3].pos.xyz - input[v0].pos.xyz);
        ns[1] = cross(input[(v0 + 2) % 3].pos.xyz - input[v1].pos.xyz, input[v0].pos.xyz - input[v1].pos.xyz);

        // Compute direction to light
        d[0] = l_pos.xyz - l_pos.w * input[v0].pos.xyz;
        d[1] = l_pos.xyz - l_pos.w * input[v1].pos.xyz;

        // CHANGE: Modify comparison logic to account for DirectX's different winding order or orientation
        // This should be in CCW order
        if (input[v1].pos.w < 1e-3 || (faces_light != (dot(ns[0], d[0]) > 0 || dot(ns[1], d[1]) > 0)))
        {
            int i0 = faces_light ? v0 : v1;
            int i1 = faces_light ? v1 : v0;
            v[0] = input[i0].pos;
            v[1] = float4(l_pos.w * input[i0].pos.xyz - l_pos.xyz, 0);
            v[2] = input[i1].pos;
            v[3] = float4(l_pos.w * input[i1].pos.xyz - l_pos.xyz, 0);

            // Emit a quad as a triangle strip
            GSOutput output;
            output.pos = v[0];
            stream.Append(output);
            output.pos = v[1];
            stream.Append(output);
            output.pos = v[2];
            stream.Append(output);
            output.pos = v[3];
            stream.Append(output);
            stream.RestartStrip();
        }
    }
}
