#pragma once
#include "Shaders/constants.hlsli"
#include "Typedefs.h"
#define CONST_QUALIFIER static const constexpr
namespace ShaderConstantCompat {
CONST_QUALIFIER u32 numInstances = NUM_INSTANCES;
CONST_QUALIFIER u32 dispMapLog2 = DISP_MAP_LOG2;
CONST_QUALIFIER u32 dispMapSize = DISP_MAP_SIZE;
CONST_QUALIFIER f32 defaultTesselation = DEFAULT_TESSELATION;
CONST_QUALIFIER u32 maxLightCount = MAX_LIGHT_COUNT;
} // namespace ShaderConstantCompat

#undef NUM_INSTANCES
#undef DISP_MAP_LOG2
#undef DISP_MAP_SIZE
#undef DEFAULT_TESSELATION
#undef MAX_LIGHT_COUNT

#undef CONST_QUALIFIER
