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
    float depth : SV_Depth;
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


output_t calculate(float4 grad, float4 Screen, float3 localPos, float2 planeCoord)
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
    
    float depth = Screen.z / Screen.w;

				

				
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


Texture2D<float2> _coneMap1 : register(t0);
Texture2D<float2> _coneMap2 : register(t1);
Texture2D<float2> _coneMap3 : register(t2);
Texture2D<float4> gradients1 : register(t3);
Texture2D<float4> gradients2 : register(t4);
Texture2D<float4> gradients3 : register(t5);

struct input_t
{
    float3 localPos : POSITION;
    //float3 normal : NORMAL;
    float4 screenPos : SV_Position;
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
        if (slope > t.y)
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
        if (slope > t.y)
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
        if (slope > t.y)
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
ConeMarchResult findIntersection_coneStepMapping(float2 uv, float3 viewDirT, uint maxSteps)
{
    float2 HMres = float2(DISP_MAP_SIZE, DISP_MAP_SIZE);
    float2 u2 = uv - viewDirT.xy / viewDirT.z;

    float3 ds = float3(u2 - uv, 1);
    ds = normalize(ds);
    float w = PrismHeight / HMres.x;
    float iz = sqrt(1.0 - ds.z * ds.z); // = length(ds.xz)
    float sc = 0;
    float3 dat = readConeMap(uv);
    int stepCount = 0;
    float zTimesSc = 0.0;

    float2 dirSign = float2(ds.x < 0 ? -1 : 1, ds.y < 0 ? -1 : 1) * 0.5 / HMres.xy;

    float relax = 0.9;
    
    while (PrismHeight - ds.z * sc > dat.x && stepCount < maxSteps)
    {
        zTimesSc = ds.z * sc;
#if CONSERVATIVE_STEP
        const float2 p = uv + ds.xy * sc;
        const float2 cellCenter = (floor(p * HMres.xy - .5) + 1) / HMres.xy;
        const float2 wall = cellCenter + dirSign;
        const float2 stepToCellBorder = (wall - p) / ds.xy;
        w = min(stepToCellBorder.x, stepToCellBorder.y) * dat.z + 1e-5;
#endif
        sc += relax * max(w, (PrismHeight - zTimesSc - dat.x) * dat.y / (-dat.y / dat.z * ds.z + iz));
        //sc += relax * ConeApprox(-(prismHeight - zTimesSc - dat.x), iz, -ds.z, dat.y / dat.z) * dat.z;
        dat = readConeMap(uv + ds.xy * sc);
        ++stepCount;
    }
    
    ConeMarchResult ret;
    float tt = ds.z * sc;
    ret.uv = (1 - tt) * uv + tt * u2;
    ret.height = tt;
    return ret;
}

output_t main2(input_t input)
{
    
    const float3 viewPos = camConstants.cameraPos;

    float3 viewDirW = normalize(input.localPos - viewPos);
    float3 T, B, N;
    //float3 normal = input.normal;
    float3 normal =
    float3(0, 1, 0);

    if (abs(normal.x) > 0.5)
    {
    // +X or -X face
        N = float3(sign(normal.x), 0, 0);
        T = float3(0, 0, -sign(normal.x));
        B = float3(0, 1, 0);
    }
    else if (abs(normal.y) > 0.5)
    {
    // +Y or -Y face
        N = float3(0, sign(normal.y), 0);
        T = float3(1, 0, 0);
        B = float3(0, 0, -sign(normal.y));
    }
    else if (abs(normal.z) > 0.5)
    {
    // +Z or -Z face
        N = float3(0, 0, sign(normal.z));
        T = float3(sign(normal.z), 0, 0);
        B = float3(0, 1, 0);
    }



    float3 viewDirT = mul(viewDirW, transpose(invMat(T, B, N))); // Transform viewDirW to tangent space
 
    //output_t output;
    //output.albedo = float4(readConeMap(float3(planeCoord.x, 0, planeCoord.y).xz) / 10., 1, 1);
        
    float2 centerOfPatch = instances[input.instanceID].offset;
    float2 halfExtentOfPatch = instances[input.instanceID].scaling;
    ConeMarchResult res = findIntersection_coneStepMapping(input.localPos.xz - center.xz, viewDirT, debugValues.maxConeStep);
    if (res.flags & BIT(2))
    {
        //output_t output;
        //output.albedo = float4(1, 0, 0, 1);
        //output.normal = float4(OctahedronNormalEncode(float3(0, 1, 0)), 0, 1);
        //output.materialValues = float4(0, 0, 0, -1);
        //return output;
        discard;
    }
    if (res.flags & BIT(1))
    {
        // miss
        discard;
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
    output_t output = calculate(grad, input.screenPos, localPos, planeCoord);

    
    // set depth
    float4 screenPos = mul(mul(float4(localPos, 1), mMatrix), camConstants.vpMatrix);
    output.depth = screenPos.z / screenPos.w;

    return output;
}


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
    const float map_height = 2.;
    float dt;
    float2 uv = localPos.xz;
    float sinB = sqrt(1. - rayv.y * rayv.y);
    int i;
    const float2 eps = 0.1;
    float3 dat;
    for (i = 0; i < maxSteps; ++i)
    {
        dat = readConeMap(uv);

        float h = dat.x;
        float tan = dat.y;
        float mult = dat.z;
        float y = rayp.y + t * rayv.y - h;
        if (y < -0.001)
        {
            break;
        }


        dt = max(
         ConeApprox(y, sinB, rayv.y, tan / mult) * mult,

            // ensure we atleast reach a new data holding texel.
            dcell(uv, rayv.xz, DISP_MAP_SIZE) * debugValues.patchSizes.r
        );
        //dt = max(ConeApprox(y, sinB, rayv.y, tan * mult), dcell(p.xz, rayv.xz, DISP_MAP_SIZE) * mult);


        acc += dt;
        t = t + dt;
        uv = rayp.xz + t * rayv.xz;
        // if we went outside the bounds then classify as a miss, the next patch should calculate it!
        if (any(abs(uv - mid) > halfExtent + eps))
        //if ((abs(p.x - mid.x) > halfExtent.x + eps.x) || (abs(p.z - mid.y) > halfExtent.y + eps.y))
        {
            res.flags = BIT(2);
            return res;
        }
    }
    res.uv = uv;
    res.height = dat.x;
    res.flags = 0;
    res.flags |= (i == maxSteps ? BIT(1) : BIT(0));

#ifdef TESTOUT
    float x = acc / 10;
    res.pos= float3(x, x, x);
#endif

    return res;
}


// Ran once per output vertex

[earlydepthstencil]
output_t main(input_t input)
{
    
    const float3 viewPos = camConstants.cameraPos;
       
    //output_t output;
    //output.albedo = float4(readConeMap(float3(planeCoord.x, 0, planeCoord.y).xz) / 10., 1, 1);
        
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

    ConeMarchResult res = ParallaxConemarch(viewPos - center, input.localPos - center, centerOfPatch, halfExtentOfPatch);
    if (res.flags & BIT(2))
    {
        //output_t output;
        //output.albedo = float4(1, 0, 0, 1);
        //output.normal = float4(OctahedronNormalEncode(float3(0, 1, 0)), 0, 1);
        //output.materialValues = float4(0, 0, 0, -1);
        //return output;
        discard;
    }
    if (res.flags & BIT(1))
    {
        // miss
        discard;
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
    output_t output = calculate(grad, input.screenPos, localPos, planeCoord);

    
    // set depth
    float4 screenPos = mul(mul(float4(localPos, 1), mMatrix), camConstants.vpMatrix);
    output.depth = screenPos.z / screenPos.w;

    return output;
}
