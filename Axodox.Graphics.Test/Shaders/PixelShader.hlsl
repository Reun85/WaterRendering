#include "common.hlsli"
Texture2D<float4> _texture : register(t0);
Texture2D<float4> _gradients : register(t1);
SamplerState _sampler : register(s0);


cbuffer CameraBuffer : register(b0)
{
    cameraConstants camConstants;
}


cbuffer Constants : register(b1)
{
    float4 mult;
    uint4 swizzleorder;
    int useTexture;
}

struct input_t
{
    float4 Screen : SV_Position;
    float3 localPos : POSITION;
    float2 TextureCoord : TEXCOORD;
};


#define ONE_OVER_4PI	0.0795774715459476

// NOTE: also defined in vertex shader
#define BLEND_START		8		// m
#define BLEND_END		200		// m

float4 main(input_t input) : SV_TARGET
{
    //return float4(input.TextureCoord, 0, 1);


    if (useTexture == 0)
    {
        float4 text = _texture.Sample(_sampler, input.TextureCoord) * float4(mult.xyz, 1);
        return Swizzle(text, swizzleorder);
    }
    

    // For now instead of envmap
    float3 ambientColor = float3(0.3, 0.3, 0.4);
    const float3 sunColor = float3(1.0, 1.0, 0.47);
    const float3 sunDir = float3(0.45, 0.1, -0.45);
    const float3 oceanColor = float3(0.1812f, 0.4678f, 0.5520f);
    
    float4 grad = _gradients.SampleLevel(_sampler, input.TextureCoord, 0);
    
    const float3 perlinFrequency = float3(1.12, 0.59, 0.23);
    const float3 perlinGradient = float3(0.014, 0.016, 0.022);

	// blend with Perlin waves
    float3 vdir = camConstants.cameraPos - input.localPos.xyz;
    float dist = length(vdir.xz);
    float factor = (BLEND_END - dist) / (BLEND_END - BLEND_START);
    float2 perl = float2(0, 0);

    factor = clamp(factor * factor * factor, 0.0, 1.0);

    //if (factor < 1.0)
    //{
    //    vec2 ptex = tex + uvParams.zw;

    //    vec2 p0 = texture(perlin, ptex * perlinFrequency.x + perlinOffset).rg;
    //    vec2 p1 = texture(perlin, ptex * perlinFrequency.y + perlinOffset).rg;
    //    vec2 p2 = texture(perlin, ptex * perlinFrequency.z + perlinOffset).rg;

    //    perl = (p0 * perlinGradient.x + p1 * perlinGradient.y + p2 * perlinGradient.z);
    //}

    //grad.xy = mix(perl, grad.xy, factor);

    float3 n = normalize(grad.xyz);
    float3 v = normalize(vdir);
    float3 l = reflect(-v, n);

    float F0 = 0.020018673;
    float F = F0 + (1.0 - F0) * pow(1.0 - dot(n, l), 5.0);

    //float3 refl = texture(envmap, l).rgb;
    float3 refl = ambientColor;

	// tweaked from ARM/Mali's sample
    float turbulence = max(1.6 - grad.w, 0.0);
    float color_mod = 1.0 + 3.0 * smoothstep(1.2, 1.8, turbulence);

    color_mod = lerp(1.0, color_mod, factor);

#if 1
	// some additional specular (Ward model)
    const float rho = 0.3;
    const float ax = 0.2;
    const float ay = 0.1;

    float3 h = sunDir + v;
    float3 x = cross(sunDir, n);
    float3 y = cross(x, n);

    float mult = (ONE_OVER_4PI * rho / (ax * ay * sqrt(max(1e-5, dot(sunDir, n) * dot(v, n)))));
    float hdotx = dot(h, x) / ax;
    float hdoty = dot(h, y) / ay;
    float hdotn = dot(h, n);

    float spec = mult * exp(-((hdotx * hdotx) + (hdoty * hdoty)) / (hdotn * hdotn));
#else
	// modified Blinn-Phong model (cheaper)
    float spec = pow(clamp(dot(sunDir, l), 0.0, 1.0), 400.0);
#endif


    float3 ret = float3(lerp(oceanColor, refl * color_mod, F) + sunColor * spec);
    return float4(ret, 1);
}