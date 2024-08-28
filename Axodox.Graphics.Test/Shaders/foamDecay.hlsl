
#include "common.hlsli"
RWTexture2D<float4> gradients : register(u0);
// .w is Jacobian matrix
RWTexture2D<float> foam : register(u1);

cbuffer time : register(b0)
{
    TimeConstants time;
}

cbuffer Test : register(b9)
{
    ComputeConstants constants;
}


[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    const float decayRate = constants.foamExponentialDecay * 60;
    const int2 loc = int2(dispatchThreadID.xy);

    const float minEPS = 0.0001;

    
    float old = foam[loc];
    old = old - decayRate * time.deltaTime * old;
    
    const float4 grad = gradients[loc];
    float newval = -grad.w;
    newval *= constants.foamMult;
    newval += constants.foamBias;
    newval = max(0, newval);
    newval *= step(minEPS, time.deltaTime);

    const float res = old + (newval > constants.foamMinValue ? newval : 0);

    foam[loc] = res;
    gradients[loc] = float4(grad.xyz, res);
}
