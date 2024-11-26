#include "../common.hlsli"
Texture2D<float4> _texture : register(t9);
SamplerState _sampler : register(s0);


cbuffer CameraBuffer : register(b0)
{
    cameraConstants camConstants;
}


cbuffer DebugBuffer : register(b9)
{
    DebugValues debugValues;
}


struct output_t
{
    float4 albedo : SV_Target0;
    float4 normal : SV_Target1;
    float4 materialValues : SV_Target2;
    float depth : SV_DepthGreaterEqual;
};



cbuffer PSProperties : register(b3)
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


output_t calculate(float4 grad, float3 localPos, float2 planeCoord, float depth)
{

    output_t output;
    
	
    if (has_flag(debugValues.flags, 6))
    {
        float2 texCoord = GetTextureCoordFromPlaneCoordAndPatch(planeCoord, debugValues.patchSizes.r);
        float4 text = _texture.Sample(_sampler, texCoord) * float4(debugValues.pixelMult.
        xyz, 1);
      
        output.albedo = text;
        return output;
    }
        
    float3 normal = normalize(grad.xyz);
    
    const float3 viewVec = camConstants.cameraPos - localPos;
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
    
				
    float foam = 0;
    if (has_flag(debugValues.flags, 2))
    {
        foam =
    lerp(0.0f, Jacobian, pow(depth, foamDepthFalloff));
    }

    //normal = lerp(normal, float3(0, 1, 0), pow(depth, NormalDepthAttenuation));
				
    // Make foam appear rougher
    float a = Roughness + foam * foamRoughnessModifier;
    				
    float3 albedo = Albedo;

    //low
    output.albedo = float4(albedo, saturate(foam));
    //high
    output.normal = float4(OctahedronNormalEncode(normal), _Fresnel, 1);
    //low
    output.materialValues = float4(a, _HeightModifier * _WavePeakScatterStrength, _ScatterShadowStrength, 1);
    output.depth = depth;
    return output;
}


Texture2D<float2> _coneMap1 : register(t0);
Texture2D<float2> _coneMap2 : register(t1);
Texture2D<float2> _coneMap3 : register(t2);
Texture2D<float4> gradients1 : register(t3);
Texture2D<float4> gradients2 : register(t4);
Texture2D<float4> gradients3 : register(t5);

struct input_t
{
    float3 localPos : POSITION;
    uint instanceID : SV_InstanceID;
};



float3 readConeMap(
    float2 uv
)
{
    float height = 0;
    float slope = 999999;
    float mult = 0;
    if (has_flag(debugValues.flags, 3))
    {
        
        float2 t = _coneMap1.SampleLevel(_sampler, GetTextureCoordFromPlaneCoordAndPatch(uv, debugValues.patchSizes.x), 0);
        height += t.x;
        float cmult = debugValues.patchSizes.x;
        
        if (slope * cmult > t.y * mult)
        {
            slope = t.y;
            mult = cmult;
        }
    }
    if (has_flag(debugValues.flags, 4))
    {
        float2 t = _coneMap2.SampleLevel(_sampler, GetTextureCoordFromPlaneCoordAndPatch(uv, debugValues.patchSizes.y), 0);
        height += t.x;
        float cmult = debugValues.patchSizes.y;
        if (slope * cmult > t.y * mult)
        {
            slope = t.y;
            mult = cmult;
        }
    }
    if (has_flag(debugValues.flags, 5))
    {
        float2 t = _coneMap3.SampleLevel(_sampler, GetTextureCoordFromPlaneCoordAndPatch(uv, debugValues.patchSizes.z), 0);
        height += t.x;
        float cmult = debugValues.patchSizes.z;
        if (slope * cmult > t.y * mult)
        {
            slope = t.y;
            mult = cmult;
        }
    }

    return float3(height, slope, mult);
}


float4 readGrad(float2 uv)
{
    float4 t1 = gradients1.SampleLevel(_sampler, GetTextureCoordFromPlaneCoordAndPatch(uv, debugValues.patchSizes.r), 0);
    float4 t2 = gradients2.SampleLevel(_sampler, GetTextureCoordFromPlaneCoordAndPatch(uv, debugValues.patchSizes.g), 0);
    float4 t3 = gradients3.SampleLevel(_sampler, GetTextureCoordFromPlaneCoordAndPatch(uv, debugValues.patchSizes.b), 0);

    return normalize(t1 + t2 + t3);
}


cbuffer ModelBuffer : register(b1)
{
    float4x4 mMatrix;
    float4x4 mINVMatrix;
    float3 center;
    float PrismHeight;
}

struct InstanceData
{
    float2 scaling;
    float2 offset;
};

cbuffer VertexBuffer : register(b2)
{
    InstanceData instances[NUM_INSTANCES];
};


float ConeApprox(
float y, float sinB, float cosB, float tanA)
{
    return tanA * y / (sinB - cosB * tanA);
}

float dcell(float p, float v, float size)
{
    float x = floor(p * size + 0.5) + 0.5 * sign(v);
    float vr = 1. / v;
    float sizer = 1. / size;
    return (x) * vr * sizer - p * vr;
}
// assume rectangular image
float dcell(float2 p, float2 v, float size)
{
    return min(dcell(p.x, v.x, size), dcell(p.y, v.y, size));
}
//https://github.com/Bundas102/robust-cone-map/tree/master
// for planes with normal (0,1,0)
// and centered around (0,0,0)

//#define TESTOUT


#define BIT(x) (1 << x)
#define CONSERVATIVE_STEP true


ConeMarchResult ParallaxConemarch(float3 viewPos, float3 localPos, float2 mid, float2 halfExtent)
{
    float3 rayp = viewPos;
    float3 rayv = localPos - rayp;
    float t = length(rayv);
    rayv /= t;
    
    float acc = 0;
    ConeMarchResult res;
    
    const uint maxSteps =
        debugValues.maxConeStep;
    float dt;
    float2 u = localPos.xz;
    float2 uv = u;
    float sinB = sqrt(1. - rayv.y * rayv.y);
    int i;
    const float2 eps = 0.3;
    float3 dat;

    res.flags = BIT(1);
    for (i = 0; i < maxSteps; ++i)
    {
        dat = readConeMap(uv);

        float h = dat.x;
        float tan = dat.y;
        float mult = dat.z;
        float slope = mult / tan;
        float y = rayp.y + t * rayv.y - h;
        if (y < -0.001)
        {
            res.flags &= ~BIT(1);
            if (i == 0)
                res.flags |= BIT(4);
            break;
        }

        // if its not a hit and we went over, just discard this fragment
        if (any(abs(uv - mid) > halfExtent + eps))
        {
            res.flags = BIT(2);
            return res;
        }

        // ensure we atleast reach a new data holding texel.
        float x1 =
            dcell(GetTextureCoordFromPlaneCoordAndPatch(uv, debugValues.patchSizes.r), rayv.xz, DISP_MAP_SIZE) * debugValues.patchSizes.r;
       // dcell(GetTextureCoordFromPlaneCoordAndPatch(uv, mult), rayv.xz, DISP_MAP_SIZE) * mult;

        float x2 =
         //debugValues.coneStepRelax * ConeApprox(y, sinB, rayv.y, tan);
            debugValues.coneStepRelax * ConeApprox(y, sinB, rayv.y, slope);

        dt = max(x1, x2);

        acc += dt;
        t = t + dt;
        uv = rayp.xz + t * rayv.xz;
    }

    res.uv = uv;
    res.height = dat.x;
    res.flags |= (i == maxSteps ? BIT(1) : BIT(0));

#ifdef TESTOUT
    float x = acc / 10;
    res.pos= float3(x, x, x);
#endif

    return res;
}


// Ran once per output vertex

output_t main(input_t input)
{
    output_t output;
    const float3 viewPos = camConstants.cameraPos;
       
    //output.albedo = float4(readConeMap(input.localPos.xz - center.xz).xz / 10., 1, 1);
    //output.normal = float4(OctahedronNormalEncode(float3(0, 1, 0)), 0, 1);
    //output.materialValues = float4(0, 0, 0, -1);
    //output.depth = input.screenPos.z / input.screenPos.w;
    //return output;
        
    float2 centerOfPatch = instances[input.instanceID].offset;
    float2 halfExtentOfPatch = instances[input.instanceID].scaling;
    //if (any(abs(abs((input.localPos - center).xz - centerOfPatch) - halfExtentOfPatch) < 0.001))
    //{
    //    output_t output;
    //    output.albedo = float4(1, 0, 0, 1);
    //    output.normal = float4(OctahedronNormalEncode(float3(0, 1, 0)), 0, 1);
    //    output.materialValues = float4(0, 0, 0, -1);
    //    return output;
    //}

    ConeMarchResult res;
    res = ParallaxConemarch(viewPos - center, input.localPos - center, centerOfPatch, halfExtentOfPatch);

    if (res.flags & BIT(4) && has_flag(debugValues.flags, 15))
    {
        output.albedo = float4(0, 0, 1, 1);
        output.normal = float4(OctahedronNormalEncode(float3(0, 1, 0)), 0, 1);
        output.materialValues = float4(0, 0, 0, -1);
        float3 localPos = float3(res.uv.x, res.height, res.uv.y);
        float4 screenPos = mul(mul(float4(localPos, 1), mMatrix), camConstants.vpMatrix);
        output.depth = screenPos.z / screenPos.w;
        return output;
    }

    if (res.flags & BIT(2))
    {
        if (has_flag(debugValues.flags, 16))
        {
            output_t output;
            output.albedo = float4(0, 1, 0, 1);
            output.normal = float4(OctahedronNormalEncode(float3(0, 1, 0)), 0, 1);
            output.materialValues = float4(0, 0, 0, -1);
            float3 localPos = float3(res.uv.x, res.height, res.uv.y);
            float4 screenPos = mul(mul(float4(localPos, 1), mMatrix), camConstants.vpMatrix);
            output.depth = screenPos.z / screenPos.w;
            return output;
        }
        else
        {
            discard;
            //res.uv = float2(10000, 10000);
        }
    }
    if (res.flags & BIT(1) && has_flag(debugValues.flags, 14))
    {
        // miss

        output.albedo = float4(1, 0, 0, 1);
        output.normal = float4(OctahedronNormalEncode(float3(0, 1, 0)), 0, 1);
        output.materialValues = float4(0, 0, 0, -1);
        float3 localPos = float3(res.uv.x, res.height, res.uv.y);
        float4 screenPos = mul(mul(float4(localPos, 1), mMatrix), camConstants.vpMatrix);
        output.depth = screenPos.z / screenPos.w;
        return output;
    }
        
    float3 localPos = float3(res.uv.x, res.height, res.uv.y);
#ifdef TESTOUT
    output_t output;
    output.albedo = float4(localPos, 1);
    output.normal = float4(OctahedronNormalEncode(float3(0, 1, 0)), 0, 1);
    output.materialValues = float4(0, 0, 0, -1);
    return output;
#endif
    float2 planeCoord = localPos.xz;
    localPos += center;
    
    float4 grad = readGrad(planeCoord);

    // Calculate color based of these attributes
    float4 screenPos = mul(mul(float4(localPos, 1), mMatrix), camConstants.vpMatrix);
    float depth = screenPos.z / screenPos.w;
    output = calculate(grad, localPos, planeCoord, depth);

    
    // set depth

    return output;
}
