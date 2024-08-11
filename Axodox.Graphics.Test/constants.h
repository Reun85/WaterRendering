#pragma once
#include "Shaders/constants.hlsli"
#include "Typedefs.h"
#define CONST_QUALIFIER static const constexpr
namespace ShaderConstantCompat {
CONST_QUALIFIER u32 numInstances = NUM_INSTANCES;
CONST_QUALIFIER u32 dispMapLog2 = DISP_MAP_LOG2;
CONST_QUALIFIER u32 dispMapSize = DISP_MAP_SIZE;
CONST_QUALIFIER u32 patchSize = PATCH_SIZE;
CONST_QUALIFIER f32 defaultTesselation = DEFAULT_TESSELATION;
CONST_QUALIFIER f32 displacementLambda = DISPLACEMENT_LAMBDA;
} // namespace ShaderConstantCompat

#undef NUM_INSTANCES
#undef DISP_MAP_LOG2
#undef DISP_MAP_SIZE
#undef PATCH_SIZE
#undef DEFAULT_TESSELATION
#undef DISPLACEMENT_LAMBDA

#undef CONST_QUALIFIER
