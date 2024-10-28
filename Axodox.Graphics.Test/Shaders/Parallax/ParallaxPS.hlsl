#include "../common.hlsli"
Texture2D<float4> _texture : register(t0);
SamplerState _sampler : register(s0);


cbuffer CameraBuffer : register(b0)
{
    cameraConstants camConstants;
}


cbuffer DebugBuffer : register(b9)
{
    DebugValues debugValues;
}




struct input_t
{
    float4 Screen : SV_POSITION;
    float3 localPos : POSITION;
    float2 planeCoord : PLANECOORD;
    float4 grad : GRADIENTS;
};

struct output_t
{
    float4 albedo;
    float4 normal;
    float4 materialValues;
};



cbuffer PSProperties : register(b2)
{
    float3 Albedo;
    float Roughness;
    float foamDepthFalloff;
    float foamRoughnessModifier;
    float NormalDepthAttenuation;
    float _HeightModifier;
    float _WavePeakScatterStrength;
    float _ScatterShadowStrength;
    float _Fresnel;
};







output_t calculate(float4 grad, float4 Screen, float3 localPos)
{

    output_t output;
    
	
           
    float3 normal = normalize(grad.xyz);
    
    const float3 viewVec = camConstants.cameraPos - localPos;
    float3 viewDir = normalize(viewVec);

    float Jacobian = grad.w;
       
    float depth = Screen.z / Screen.w;

				

				
    float foam = 0;
    foam =
    lerp(0.0f, Jacobian, pow(depth, foamDepthFalloff));

    normal = lerp(normal, float3(0, 1, 0), pow(depth, NormalDepthAttenuation));



				
    // Make foam appear rougher
    float a = Roughness + foam * foamRoughnessModifier;

   
    				
    float3 albedo = Albedo;

    //low
    output.albedo = float4(albedo, saturate(foam));
    //high
    output.normal = float4(OctahedronNormalEncode(normal), _Fresnel, 1);
    //low
    output.materialValues = float4(a, _HeightModifier * _WavePeakScatterStrength, _ScatterShadowStrength, 1);
    return output;
    
}


Texture2D<float2> _coneMap1 : register(t0);
Texture2D<float2> _coneMap2 : register(t1);
Texture2D<float2> _coneMap3 : register(t2);
Texture2D<float4> gradients1 : register(t3);
Texture2D<float4> gradients2 : register(t4);
Texture2D<float4> gradients3 : register(t5);

struct DS_OUTPUT
{
    float4 Position : SV_POSITION;
    float3 localPos : POSITION;
    float2 TexCoord : PLANECOORD;
    float4 grad : GRADIENTS;
};

struct HS_OUTPUT_PATCH
{
    float3 localPos : POSITION;
    float2 TexCoord : PLANECOORD;
};


struct HS_CONSTANT_DATA_OUTPUT
{
    float edges[4] : SV_TessFactor;
    float inside[2] : SV_InsideTessFactor;
};


struct ConeMapData
{
    float height;
    float slope;
};


float3 localPos = input.localPos;
float2 planeCoord = input.planeCoord;
const float3 viewPos = camConstants.cameraPos;


ConeMapData readConeMap(
    float2 uv,
)
{
    float2 t1 = _coneMap1.SampleLevel(_sampler, GetTextureCoordFromPlaneCoordAndPatch(uv, debugValues.patchSizes.r), 0);
    float2 t2 = _coneMap2.SampleLevel(_sampler, GetTextureCoordFromPlaneCoordAndPatch(uv, debugValues.patchSizes.g), 0);
    float2 t3 = _coneMap3.SampleLevel(_sampler, GetTextureCoordFromPlaneCoordAndPatch(uv, debugValues.patchSizes.b), 0);
    ConeMapData r;
    r.height = t1.x + t2.x + t3.x;
    r.slope = min(t1.y, min(t2.y, t3.y));

    return r;
}

float4 readGrad(float2 uv)
{
    float4 t1 = gradients1.SampleLevel(_sampler, GetTextureCoordFromPlaneCoordAndPatch(uv, debugValues.patchSizes.r), 0);
    float4 t2 = gradients2.SampleLevel(_sampler, GetTextureCoordFromPlaneCoordAndPatch(uv, debugValues.patchSizes.g), 0);
    float4 t3 = gradients3.SampleLevel(_sampler, GetTextureCoordFromPlaneCoordAndPatch(uv, debugValues.patchSizes.b), 0);

    return t1 + t2 + t3;
}


float parallaxConeStepping(float2 uv, float3 localPos, float3 viewPos, float coneStepScale, int maxSteps)
{
    float3 viewDir = normalize(viewPos - localPos);
    float height = 0.0;

    // Step variables
    float stepSize = 1.0 / maxSteps;
    float2 deltaUV = viewDir.xy * coneStepScale * stepSize;
    float2 currentUV = uv;

    // Perform cone stepping
    for (int i = 0; i < maxSteps; i++)
    {
        // Read the cone map data at the current UV
        ConeMapData coneData = readConeMap(currentUV);

        // Calculate expected height at this step based on cone slope
        float expectedHeight = height + coneData.slope;

        // If the view depth exceeds the height at the current UV, we break out of the loop
        if (viewDir.z * height > expectedHeight)
        {
            break;
        }

        // Accumulate height and advance UV
        height += coneData.height * stepSize;
        currentUV += deltaUV;
    }

    return currentUV;
}


// Ran once per output vertex
output_t main(input_t input) : SV_TARGET
{
    const bool apply_disp = true;
    output_t output;
    
    float3 localPos = input.localPos;
    float2 planeCoord = input.planeCoord;
    const float3 viewPos = camConstants.cameraPos;


    planeCoord = parallaxConeStepping(planeCoord, localPos, viewPos, debugValues.patchSizes.r, debugValues.maxConeSteps);

    
    float4 grad = readGrad(planeCoord);
    
    
    
    return calculate(grad, input.Screen, localPos);
}

