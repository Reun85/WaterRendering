#include "common.hlsli"


Texture2D<float4> texture : register(t3);

SamplerState _sampler : register(s0);


struct input_t
{
    float4 position : SV_POSITION;
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
    
	
            
    float3 normal = float3(0, 0, 0);
    
    				
    float3 albedo = float3(1, 1, 1);

    //low
    output.albedo = float4(albedo, 0);
    //high
    output.normal = float4(OctahedronNormalEncode(normal), 0, 1);
    //high
    output.materialValues = float4(0, 0, 0, 0);
    return output;
    
}
