
#include "common.hlsli"

cbuffer CameraBuffer : register(b0)
{
    cameraConstants camConstants;
}





struct input_t
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 textureCoord : TEXCOORD;
};

struct output_t
{
    float4 albedo;
    float4 normal;
    float4 materialValues;
};

output_t main(input_t input) : SV_TARGET
{

    output_t output;
    
	
            
    float3 normal = normalize(input.normal);
    
    				
    float3 albedo = float3(1, 1, 1);

    //low
    output.albedo = float4(albedo, 0);
    //high
    output.normal = float4(OctahedronNormalEncode(normal), 0, 1);
    //high
    output.materialValues = float4(0, 0, 0, 0);
    return output;
    
}
