#pragma once
#include "pch.h"
#include "Defaults.h"
#include "Simulation.h"

namespace Reun {
using namespace Axodox::Infrastructure;
using namespace Axodox::Storage;
using namespace Axodox::Threading;

struct DebugValues {
  enum class DebugTextureDisplay : u8 {
    DisplacementHighest = 0,
    DisplacementMedium,
    DisplacementLowest,
    GradientsHighest,
    GradientsMedium,
    GradientsLowest,
  };
  std::optional<DebugTextureDisplay> debugTextureMode = std::nullopt;

  enum class DrawTechnology : u8 { Tesselation, Parallax, PrismParallax };
  DrawTechnology drawMethod = DrawTechnology::Tesselation;

  XMFLOAT4 pixelMult = XMFLOAT4(1, 1, 1, 1);
  XMUINT4 swizzleorder = XMUINT4(0, 1, 2, 3);
  XMFLOAT4 blendDistances = XMFLOAT4(150.f, 300, 1000, 5000);
  XMFLOAT3 foamColor = XMFLOAT3(1, 1, 1);

  std::array<bool, 31 - 0 + 1> DebugBits{false, false, true,
                                         true,  false, false};

  bool enableSSR = false;
  bool lockQuadTree = false;
  int maxConeStep = 40;
  float prismHeight = 2;
  float coneStepRelax = 0.9f;
  bool conecreater = false;

  bool calculateParallax() const {
    return drawMethod == DrawTechnology::Parallax ||
           drawMethod == DrawTechnology::PrismParallax;
  }
  std::array<bool, 3> getChannels() {
    // I am sorry, I am lazy
    return {DebugBits[3], DebugBits[4], DebugBits[5]};
  }
  RasterizerFlags rasterizerFlags = RasterizerFlags::CullClockwise;
  void DrawImGui(NeedToDo &out, bool exclusiveWindow = true);

private:
  void CullingImGuiDraw(NeedToDo &out);
  void useTextureImGuiDraw();
};

struct DebugGPUBufferStuff {
  XMFLOAT4 pixelMult = XMFLOAT4(0, 0, 0, 0);
  XMFLOAT4 blendDistances = XMFLOAT4(0, 0, 0, 0);

  XMUINT4 swizzleOrder = XMUINT4(0, 0, 0, 0);
  XMFLOAT4 foamInfo = XMFLOAT4(0, 0, 0, 0);
  XMFLOAT3 patchSizes = XMFLOAT3(0, 0, 0);
  // 0: use displacement
  // 1: use normal
  // 2: use foam
  // 3: use channel1
  // 4: use channel2
  // 5: use channel3
  // 6: display texture instead of shader
  // 7: transform texture values from [-1,1] to [0,1]
  u32 flags = 0; // Its here because padding
  float EnvMapMult = 0.;
  int maxConeStep = 0;
  float coneStepRelax = 0.;
};
DebugGPUBufferStuff From(const DebugValues &deb, const SimulationData &simData);
struct RuntimeSettings {
  bool timeRunning = true;
  bool showImgui = true;
  XMFLOAT4 clearColor = DefaultsValues::App::clearColor;
  void DrawImGui([[maybe_unused]] NeedToDo &out, bool exclusiveWindow = false);
};
} // namespace Reun
