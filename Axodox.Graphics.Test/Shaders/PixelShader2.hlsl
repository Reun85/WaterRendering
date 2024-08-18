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


#define PI 3.14159265359
float SchlickFresnel(float3 normal, float3 viewDir)
{
				// 0.02f comes from the reflectivity bias of water kinda idk it's from a paper somewhere i'm not gonna link it tho lmaooo
    return 0.02f + (1 - 0.02f) * (pow(1 - max(dot(normal, viewDir), 0), 5.0f));
}

float SmithMaskingBeckmann(float3 H, float3 S, float roughness)
{
    float hdots = max(0.001f, max(dot(H, S), 0));
    float a = hdots / (roughness * sqrt(1 - hdots * hdots));
    float a2 = a * a;

    return a < 1.6f ? (1.0f - 1.259f * a + 0.396f * a2) / (3.535f * a + 2.181 * a2) : 0.0f;
}

float Beckmann(float ndoth, float roughness)
{
    float exp_arg = (ndoth * ndoth - 1) / (roughness * roughness * ndoth * ndoth);

    return exp(exp_arg) / (PI * roughness * roughness * ndoth * ndoth * ndoth * ndoth);
}
float4 main(input_t input, bool frontFacing : SV_IsFrontFace) : SV_TARGET
{


    if (useTexture == 0)
    {
        float4 text = _texture.Sample(_sampler, input.TextureCoord) * float4(mult.xyz, 1);
        return Swizzle(text, swizzleorder);
    }
    
    
// Colors
    const float3 sunColor = float3(1.0, 1.0, 0.47);
    const float3 sunDir = float3(0.45, 0.1, -0.45);
    const float3 waterColor = float3(0.1812f, 0.4678f, 0.5520f);

    
    // Other constants
    const float3 _DiffuseReflectance = float3(0.1f, 0.1f, 0.1f);
    const float _FresnelNormalStrength = 1.0f;
    const float _FresnelShininess = 5.0f;
    const float _FresnelBias = 0.02f;
    const float _FresnelStrength = 1.0f;
    const float3 _FresnelColor = float3(0.6f, 0.8f, 1.0f);
    const float3 _SpecularReflectance = float3(0.2f, 0.2f, 0.2f);
    const float _SpecularNormalStrength = 0.5f;
    const float _Shininess = 50.0f;
    const float3 _LightColor0 = float3(1.0f, 1.0f, 1.0f);
    const float3 _TipColor = float3(0.8f, 0.9f, 1.0f);
    const float _TipAttenuation = 10.0f;
    const float3 _Ambient = float3(0.05f, 0.05f, 0.1f);
     
    float4 grad = _gradients.Sample(_sampler, input.TextureCoord);

    float3 normal = normalize(grad.xyz);
    float Jacobian = grad.w;
    normal *= frontFacing ? -1 : 1;

    
    return grad;
	
#if 0
    
    float3 lightDir = -normalize(_SunDirection);
    float3 viewDir = normalize(_WorldSpaceCameraPos - f.data.worldPos);
    float3 halfwayDir = normalize(lightDir + viewDir);
    float depth = f.data.depth;
    float LdotH = DotClamped(lightDir, halfwayDir);
    float VdotH = DotClamped(viewDir, halfwayDir);
				
				
    float4 displacementFoam1 = UNITY_SAMPLE_TEX2DARRAY(_DisplacementTextures, float3(f.data.uv * _Tile0, 0)) * _DebugLayer0;
    displacementFoam1.a += _FoamSubtract0;
    float4 displacementFoam2 = UNITY_SAMPLE_TEX2DARRAY(_DisplacementTextures, float3(f.data.uv * _Tile1, 1)) * _DebugLayer1;
    displacementFoam2.a += _FoamSubtract1;
    float4 displacementFoam3 = UNITY_SAMPLE_TEX2DARRAY(_DisplacementTextures, float3(f.data.uv * _Tile2, 2)) * _DebugLayer2;
    displacementFoam3.a += _FoamSubtract2;
    float4 displacementFoam4 = UNITY_SAMPLE_TEX2DARRAY(_DisplacementTextures, float3(f.data.uv * _Tile3, 3)) * _DebugLayer3;
    displacementFoam4.a += _FoamSubtract3;
    float4 displacementFoam = displacementFoam1 + displacementFoam2 + displacementFoam3 + displacementFoam4;

				
    float2 slopes1 = UNITY_SAMPLE_TEX2DARRAY(_SlopeTextures, float3(f.data.uv * _Tile0, 0)) * _DebugLayer0;
    float2 slopes2 = UNITY_SAMPLE_TEX2DARRAY(_SlopeTextures, float3(f.data.uv * _Tile1, 1)) * _DebugLayer1;
    float2 slopes3 = UNITY_SAMPLE_TEX2DARRAY(_SlopeTextures, float3(f.data.uv * _Tile2, 2)) * _DebugLayer2;
    float2 slopes4 = UNITY_SAMPLE_TEX2DARRAY(_SlopeTextures, float3(f.data.uv * _Tile3, 3)) * _DebugLayer3;
    float2 slopes = slopes1 + slopes2 + slopes3 + slopes4;

				
    slopes *= _NormalStrength;
    float foam = lerp(0.0f, saturate(displacementFoam.a), pow(depth, _FoamDepthAttenuation));

#define NEW_LIGHTING
#ifdef NEW_LIGHTING
    float3 macroNormal = float3(0, 1, 0);
    float3 mesoNormal = normalize(float3(-slopes.x, 1.0f, -slopes.y));
    mesoNormal = normalize(lerp(float3(0, 1, 0), mesoNormal, pow(saturate(depth), _NormalDepthAttenuation)));
    mesoNormal = normalize(UnityObjectToWorldNormal(normalize(mesoNormal)));

    float NdotL = DotClamped(mesoNormal, lightDir);

				
    float a = _Roughness + foam * _FoamRoughnessModifier;
    float ndoth = max(0.0001f, dot(mesoNormal, halfwayDir));

    float viewMask = SmithMaskingBeckmann(halfwayDir, viewDir, a);
    float lightMask = SmithMaskingBeckmann(halfwayDir, lightDir, a);
				
    float G = rcp(1 + viewMask + lightMask);

    float eta = 1.33f;
    float R = ((eta - 1) * (eta - 1)) / ((eta + 1) * (eta + 1));
    float thetaV = acos(viewDir.y);

    float numerator = pow(1 - dot(mesoNormal, viewDir), 5 * exp(-2.69 * a));
    float F = R + (1 - R) * numerator / (1.0f + 22.7f * pow(a, 1.5f));
    F = saturate(F);
				
    float3 specular = _SunIrradiance * F * G * Beckmann(ndoth, a);
    specular /= 4.0f * max(0.001f, DotClamped(macroNormal, lightDir));
    specular *= DotClamped(mesoNormal, lightDir);

    float3 envReflection = texCUBE(_EnvironmentMap, reflect(-viewDir, mesoNormal)).rgb;
    envReflection *= _EnvironmentLightStrength;

    float H = max(0.0f, displacementFoam.y) * _HeightModifier;
    float3 scatterColor = _ScatterColor;
    float3 bubbleColor = _BubbleColor;
    float bubbleDensity = _BubbleDensity;

				
    float k1 = _WavePeakScatterStrength * H * pow(DotClamped(lightDir, -viewDir), 4.0f) * pow(0.5f - 0.5f * dot(lightDir, mesoNormal), 3.0f);
    float k2 = _ScatterStrength * pow(DotClamped(viewDir, mesoNormal), 2.0f);
    float k3 = _ScatterShadowStrength * NdotL;
    float k4 = bubbleDensity;

    float3 scatter = (k1 + k2) * scatterColor * _SunIrradiance * rcp(1 + lightMask);
    scatter += k3 * scatterColor * _SunIrradiance + k4 * bubbleColor * _SunIrradiance;

				
    float3 output = (1 - F) * scatter + specular + F * envReflection;
    output = max(0.0f, output);
    output = lerp(output, _FoamColor, saturate(foam));

#else
    slopes *= _NormalStrength;
    float3 normal = normalize(float3(-slopes.x, 1.0f, -slopes.y));
    normal = normalize(UnityObjectToWorldNormal(normalize(normal)));

    float ndotl = DotClamped(lightDir, normal);

    float3 diffuseReflectance = _DiffuseReflectance / PI;
    float3 diffuse = _LightColor0.rgb * ndotl * diffuseReflectance;

				// Schlick Fresnel
    float3 fresnelNormal = normal;
    fresnelNormal.xz *= _FresnelNormalStrength;
    fresnelNormal = normalize(fresnelNormal);
    float base = 1 - dot(viewDir, fresnelNormal);
    float exponential = pow(base, _FresnelShininess);
    float R = exponential + _FresnelBias * (1.0f - exponential);
    R *= _FresnelStrength;
				
    float3 fresnel = _FresnelColor * R;
                
    if (_UseEnvironmentMap)
    {
        float3 reflectedDir = reflect(-viewDir, normal);
        float3 skyCol = texCUBE(_EnvironmentMap, reflectedDir).rgb;
        float3 sun = _SunColor * pow(max(0.0f, DotClamped(reflectedDir, lightDir)), 500.0f);

        fresnel = skyCol.rgb * R;
        fresnel += sun * R;
    }


    float3 specularReflectance = _SpecularReflectance;
    float3 specNormal = normal;
    specNormal.xz *= _SpecularNormalStrength;
    specNormal = normalize(specNormal);
    float spec = pow(DotClamped(specNormal, halfwayDir), _Shininess) * ndotl;
    float3 specular = _LightColor0.rgb * specularReflectance * spec;

				// Schlick Fresnel but again for specular
    base = 1 - DotClamped(viewDir, halfwayDir);
    exponential = pow(base, 5.0f);
    R = exponential + _FresnelBias * (1.0f - exponential);

    specular *= R;
				

    float3 output = _Ambient + diffuse + specular + fresnel;
    output = lerp(output, _TipColor, saturate(foam));
#endif


    if (_DebugTile0)
    {
        output = cos(f.data.uv.x * _Tile0 * PI) * cos(f.data.uv.y * _Tile0 * PI);
    }

    if (_DebugTile1)
    {
        output = cos(f.data.uv.x * _Tile1) * 1024 * cos(f.data.uv.y * _Tile1) * 1024;
    }

    if (_DebugTile2)
    {
        output = cos(f.data.uv.x * _Tile2) * 1024 * cos(f.data.uv.y * _Tile2) * 1024;
    }

    if (_DebugTile3)
    {
        output = cos(f.data.uv.x * _Tile3) * 1024 * cos(f.data.uv.y * _Tile3) * 1024;
    }
				
    return float4(output, 1.0f);
    
#endif
    
}
