#include "common.hlsli"
Texture2D<float4> _texture : register(t0);
SamplerState _sampler : register(s0);

Texture2D<float4> gradients1 : register(t3);
Texture2D<float4> gradients2 : register(t4);
Texture2D<float4> gradients3 : register(t5);


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
    uint instanceID : INSTANCEID;
};

struct output_t
{
    float4 albedo;
    float4 normal;
    float4 materialValues;
};
struct InstanceData
{
    float2 scaling;
    float2 offset;
};
cbuffer VertexBuffer : register(b2)
{
    InstanceData instances[NUM_INSTANCES];
};


cbuffer PSProperties : register(b4)
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
float4 readGrad(float2 uv)
{
    float4 t1 = float4(0, 0, 0, 0);
    float4 t2 = float4(0, 0, 0, 0);
    float4 t3 = float4(0, 0, 0, 0);
    if (has_flag(debugValues.flags, 3))
        t1 = gradients1.SampleLevel(_sampler, GetTextureCoordFromPlaneCoordAndPatch(uv, debugValues.patchSizes.r), 0);
    if (has_flag(debugValues.flags, 4))
        t2 = gradients2.SampleLevel(_sampler, GetTextureCoordFromPlaneCoordAndPatch(uv, debugValues.patchSizes.g), 0);
    if (has_flag(debugValues.flags, 5))
        t3 = gradients3.SampleLevel(_sampler, GetTextureCoordFromPlaneCoordAndPatch(uv, debugValues.patchSizes.b), 0);

    return float4(normalize(t1.rgb + t2.rgb + t3.rgb), t1.w + t2.w + t3.w);
}



output_t main(input_t input, bool frontFacing : SV_IsFrontFace) : SV_TARGET
{
 
            
    float2 centerOfPatch = instances[input.instanceID].offset;
    float2 halfExtentOfPatch = instances[input.instanceID].scaling / 2;
    if (any(abs(abs((input.localPos).xz - centerOfPatch) - halfExtentOfPatch) < 0.2))
    {
        output_t output;
        output.albedo = float4(0, 0, 1, 1);
        output.normal = float4(OctahedronNormalEncode(float3(0, 1, 0)), 0, 1);
        output.materialValues = float4(0, 0, 0, -1);
        return output;
    }
    else
    {
        output_t output;
        output.albedo = float4(0, 0, 0, 1);
        output.normal = float4(OctahedronNormalEncode(float3(0, 1, 0)), 0, 1);
        output.materialValues = float4(0, 0, 0, -1);
        return output;

    }


    
    
    
    output_t output;
    
	
    if (has_flag(debugValues.flags, 6))
    {
        float2 texCoord = GetTextureCoordFromPlaneCoordAndPatch(input.planeCoord, debugValues.patchSizes.r);
        float4 text = _texture.Sample(_sampler, texCoord) * float4(debugValues.pixelMult.
        xyz, 1);
      
        output.albedo = text;
        return output;
    }
    float4 grad = readGrad(input.planeCoord);
        
    float3 normal = normalize(grad.xyz);
    
    const float3 viewVec = camConstants.cameraPos - input.localPos;
    float3 viewDir = normalize(viewVec);

    if (has_flag(debugValues.flags, 24))
    {
        output.albedo =
         float4(0, 1, 0, 1);
        if (dot(normal, viewDir) < 0)
            output.albedo = float4(1, 0, 0, 1);
        
        return output;
    }

    //if (dot(normal, viewDir) < 0)
    //{
    //    normal *= -1;
    //}

    float Jacobian = grad.w;
    if (has_flag(debugValues.flags, 25))
    {
        output.albedo =
         float4(Jacobian, Jacobian, Jacobian, 1);
        return output;
    }
    
    float depth = input.Screen.z / input.Screen.w;

				

				
    float foam = 0;
    if (has_flag(debugValues.flags, 2))
    {
        foam =
    lerp(0.0f, Jacobian, pow(depth, foamDepthFalloff));
    }

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
