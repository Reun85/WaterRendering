#pragma once
#include "GraphicsPipeline.h"
#include "DebugValues.h"
#include "Camera.h"
struct RuntimeValues;
struct SimulationData;
struct NeedToDo;
/// <summary>
/// uses static data
/// </summary>
void ShowImguiLoaderConfig(
    DebugValues &debugValues, SimulationData &simData,
    WaterGraphicRootDescription::WaterPixelShaderData &waterData,
    PixelLighting &sunData,
    DeferredShading::DeferredShaderBuffers &deferredData,
    RuntimeSettings &settings, Camera &cam, NeedToDo &beforeNextFrame,
    bool exclusiveWindow);
