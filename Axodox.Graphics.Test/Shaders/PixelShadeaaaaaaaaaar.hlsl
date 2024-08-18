
#include "common.hlsli"
Texture2D<float4> _texture : register(t0);
TextureCube<float4> _skybox : register(t1);
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
    float4 Screen : SV_Position;
    float3 localPos : POSITION;
    float2 planeCoord : PLANECOORD;
    float4 grad : GRADIENTS;
};



cbuffer MaterialProperties : register(b2)
{
    float3 SurfaceColor;
    float Roughness;
    float SubsurfaceScattering;
    float Sheen;
    float SheenTint;
    float Anisotropic;
    float SpecularStrength;
    float Metallic;
    float SpecularTint;
    float ClearCoat;
    float ClearCoatGloss;
};


float luminance(float3 color)
{
    return dot(color, float3(0.299f, 0.587f, 0.114f));
}

float SchlickFresnel(float x)
{
    x = saturate(1.0f - x);
    float x2 = x * x;

    return x2 * x2 * x; // While this is equivalent to pow(1 - x, 5) it is two less mult instructions
}

        // Isotropic Generalized Trowbridge Reitz with gamma == 1
float GTR1(float ndoth, float a)
{
    float a2 = a * a;
    float t = 1.0f + (a2 - 1.0f) * ndoth * ndoth;
    return (a2 - 1.0f) / (PI * log(a2) * t);
}

        // Anisotropic Generalized Trowbridge Reitz with gamma == 2. This is equal to the popular GGX distribution.
float AnisotropicGTR2(float ndoth, float hdotx, float hdoty, float ax, float ay)
{
    return rcp(PI * ax * ay * sqr(sqr(hdotx / ax) + sqr(hdoty / ay) + sqr(ndoth)));
}

        // Isotropic Geometric Attenuation Function for GGX. This is technically different from what Disney uses, but it's basically the same.
float SmithGGX(float alphaSquared, float ndotl, float ndotv)
{
    float a = ndotv * sqrt(alphaSquared + ndotl * (ndotl - alphaSquared * ndotl));
    float b = ndotl * sqrt(alphaSquared + ndotv * (ndotv - alphaSquared * ndotv));

    return 0.5f / (a + b);
}

        // Anisotropic Geometric Attenuation Function for GGX.
float AnisotropicSmithGGX(float ndots, float sdotx, float sdoty, float ax, float ay)
{
    return rcp(ndots + sqrt(sqr(sdotx * ax) + sqr(sdoty * ay) + sqr(ndots)));
}

struct BRDFResults
{
    float3 diffuse;
    float3 specular;
    float3 clearcoat;
};

BRDFResults DisneyBRDF(float3 baseColor, float3 L, float3 V, float3 N, float3 X, float3 Y)
{
    BRDFResults output;
    output.diffuse = 0.0f;
    output.specular = 0.0f;
    output.clearcoat = 0.0f;

    float3 H = normalize(L + V); // Microfacet normal of perfect reflection

    float ndotl = dot(N, L);
    float ndotv = DotClamped(N, V);
    float ndoth = DotClamped(N, H);
    float ldoth = DotClamped(L, H);

    float3 surfaceColor = baseColor * SurfaceColor;

    float Cdlum = luminance(surfaceColor);

    float3 Ctint = Cdlum > 0.0f ? surfaceColor / Cdlum : 1.0f;
    float3 Cspec0 = lerp(SpecularStrength * 0.08f * lerp(1.0f, Ctint, SpecularTint), surfaceColor, Metallic);
    float3 Csheen = lerp(1.0f, Ctint, SheenTint);


            // Disney Diffuse
    float FL = SchlickFresnel(ndotl);
    float FV = SchlickFresnel(ndotv);

    float Fss90 = ldoth * ldoth * Roughness;
    float Fd90 = 0.5f + 2.0f * Fss90;

    float Fd = lerp(1.0f, Fd90, FL) * lerp(1.0f, Fd90, FV);

            // Subsurface Diffuse (Hanrahan-Krueger brdf approximation)

    float Fss = lerp(1.0f, Fss90, FL) * lerp(1.0f, Fss90, FV);
    float ss = 1.25f * (Fss * (rcp(ndotl + ndotv) - 0.5f) + 0.5f);

            // Specular
    float alpha = Roughness;
    float alphaSquared = alpha * alpha;

            // Anisotropic Microfacet Normal Distribution (Normalized Anisotropic GTR gamma == 2)
    float aspectRatio = sqrt(1.0f - Anisotropic * 0.9f);
    float alphaX = max(0.001f, alphaSquared / aspectRatio);
    float alphaY = max(0.001f, alphaSquared * aspectRatio);
    float Ds = AnisotropicGTR2(ndoth, dot(H, X), dot(H, Y), alphaX, alphaY);

            // Geometric Attenuation
    float GalphaSquared = sqr(0.5f + Roughness * 0.5f);
    float GalphaX = max(0.001f, GalphaSquared / aspectRatio);
    float GalphaY = max(0.001f, GalphaSquared * aspectRatio);
    float G = AnisotropicSmithGGX(ndotl, dot(L, X), dot(L, Y), GalphaX, GalphaY);
    G *= AnisotropicSmithGGX(ndotv, dot(V, X), dot(V, Y), GalphaX, GalphaY); // specular brdf denominator (4 * ndotl * ndotv) is baked into output here (I assume at least)  

            // Fresnel Reflectance
    float FH = SchlickFresnel(ldoth);
    float3 F = lerp(Cspec0, 1.0f, FH);

            // Sheen
    float3 Fsheen = FH * Sheen * Csheen;

            // Clearcoat (Hard Coded Index Of Refraction -> 1.5f -> F0 -> 0.04)
    float Dr = GTR1(ndoth, lerp(0.1f, 0.001f, ClearCoatGloss)); // Normalized Isotropic GTR Gamma == 1
    float Fr = lerp(0.04, 1.0f, FH);
    float Gr = SmithGGX(ndotl, ndotv, 0.25f);

            
    output.diffuse = (1.0f / PI) * (lerp(Fd, ss, SubsurfaceScattering) * surfaceColor + Fsheen) * (1 - Metallic);
    output.specular = Ds * F * G;
    output.clearcoat = 0.25f * ClearCoat * Gr * Fr * Dr;

    return output;
}



float4 main(input_t input, bool isFrontFacing : SV_IsFrontFace) : SV_TARGET
{

    if (has_flag(debugValues.flags, 6))
    {
        float2 texCoord = GetTextureCoordFromPlaneCoordAndPatch(input.planeCoord, debugValues.patchSizes.r);
        float4 text = _texture.Sample(_sampler, texCoord) * float4(debugValues.pixelMult.
        xyz, 1);
        if (has_flag(debugValues.flags, 7))
        {
            text.xyz = (text.xyz + 1) / 2;
        }

        return text;
        return Swizzle(text, debugValues.swizzleOrder);
    }
    
    

    

    
    
    float3 normal = input.grad.xyz;
    // Why is it backwards??????????
    normal = isFrontFacing ? -normal : normal;

    float3 viewDir = normalize(camConstants.cameraPos - input.localPos);
    if (has_flag(debugValues.flags, 24))
    {
        if (dot(normal, viewDir) < 0)
            return float4(1, 0, 0, 1);
        return float4(0, 1, 0, 1);
    }
    if (dot(normal, viewDir) < 0)
    {
        normal *= -1;
    }

    if (has_flag(debugValues.flags, 7))
    {
        return float4(normal / 2 + 0.5, 1);
    }




    float Jacobian = input.grad.w;
     

    
    float3 LightDirection = lights[0].lightPos.xyz;
    float3 LightColor = lights[0].lightColor.xyz;
    float LightIntensity = lights[0].lightColor.w;
    
    float3 arbitrary = float3(0.0f, 1.0f, 1.0f); // An arbitrary vector for cross product
    float3 X = normalize(cross(arbitrary, normal)); // Tangent vector
    float3 Y = cross(normal, X);
    BRDFResults
    reflection = DisneyBRDF(float3(1, 1, 1), LightDirection, viewDir, normal, X, Y);

    float3 output = LightIntensity * LightColor * (reflection.diffuse + reflection.specular + reflection.clearcoat);
    output *= DotClamped(normal, viewDir);
    return float4(output, 1);
}
