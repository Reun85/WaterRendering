
#include "common.hlsli"
RWTexture2D<float4> gradients : register(u0);
// .w is Jacobian matrix
RWTexture2D<float> foam : register(u1);

cbuffer time : register(b0)
{
    TimeConstants time;
}


[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    const float decayRate = FOAM_EXPONENTIAL_DECAY*1000;
    const int2 loc = int2(dispatchThreadID.xy);
    const float4 grad = gradients[loc];
    const float newval = grad.w < 0 ? -grad.w : 0;
    float old = foam[loc];
    old = old - decayRate/1000.f * old;
    const float res = old+ newval;
    foam[loc] = res;
    gradients[loc] = float4(grad.xyz, res);
}
