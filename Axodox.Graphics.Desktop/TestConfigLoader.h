#pragma once
#include "pch.h"

namespace Reun {
struct DebugValues;
class Camera;
struct RuntimeValues;
struct RuntimeSettings;
struct SimulationData;
struct NeedToDo;
namespace Graphics {
struct DeferredShading;
struct DeferredShading::DeferredShaderBuffers;
struct TesselationGraphicRootDescription;
struct TesselationGraphicRootDescription::WaterPixelShaderData;

struct PixelLighting;
} // namespace Graphics

/// <summary>
/// uses static data
/// </summary>
void ShowImguiLoaderConfig(
    DebugValues &debugValues, SimulationData &simData,
    Graphics::TesselationGraphicRootDescription::WaterPixelShaderData
        &waterData,
    Graphics::PixelLighting &sunData,
    Graphics::DeferredShading::DeferredShaderBuffers &deferredData,
    RuntimeSettings &settings, Camera &cam, NeedToDo &beforeNextFrame);
} // namespace Reun
