#pragma once
#include "pch.h"

struct DebugValues;
struct WaterGraphicRootDescription;
struct WaterGraphicRootDescription::WaterPixelShaderData;

struct PixelLighting;
class Camera;
struct RuntimeValues;
struct RuntimeSettings;
struct SimulationData;
struct NeedToDo;
struct DeferredShading;
struct DeferredShading::DeferredShaderBuffers;

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
