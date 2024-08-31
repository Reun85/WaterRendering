
#include "common.hlsli"
Texture2D<float4> _albedo : register(t0);
Texture2D<float4> _normal : register(t1);
Texture2D<float4> _position : register(t2);
Texture2D<float4> _materialValues : register(t3);

SamplerState _sampler : register(s0);

#define PI 3.14159265359

cbuffer CameraBuffer : register(b0)
{
    cameraConstants camConstants;
}


cbuffer DebugBuffer : register(b9)
{
    DebugValues debugValues;
}



struct PixelLightData
{
    float4 lightPos;
    float4 lightColor;
};
cbuffer PixelLighting : register(b1)
{
    PixelLightData lights[MAX_LIGHT_COUNT];
    int lightCount;
};



struct input_t
{
    float4 screen : SV_Position;
    float2 texCoord : TEXCOORD;
};

float4 main(input_t input) : SV_TARGET
{
    return _albedo.Sample(_sampler, input.texCoord);
}
