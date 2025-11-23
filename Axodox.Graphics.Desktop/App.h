#pragma once
#include "pch.h"

#include "ComputePipeline.h"
#include "DebugValues.h"
#include "GraphicsPipeline.h"
#include "Parallax.h"
#include "QuadTree.h"
#include "ShadowVolume.h"
#include "Simulation.h"
#include "SkyboxPipeline.hpp"

#include "Gui.h"
#include "AppShared.h"

namespace Reun {

using namespace Axodox::Infrastructure;
using namespace Axodox::Storage;
using namespace Axodox::Threading;
using namespace winrt::Windows::UI::Core;

using namespace DirectX;
struct TimeData {
  // used by camera etc, will always be !=0
  float trueDeltaTime = 0.f;
  // should be used for simulation, action
  float deltaTime = 0.f;
  float timeSinceLaunch = 0.f;
};

struct Descriptors {
  PipelineStateProvider pipelineStateProvider_;
  GroupedResourceAllocator groupedResourceAllocator;
  ResourceUploader resourceUploader;
  CommonDescriptorHeap commonDescriptorHeap;
  DepthStencilDescriptorHeap depthStencilDescriptorHeap;
  RenderTargetDescriptorHeap renderTargetDescriptorHeap;
  CommittedResourceAllocator committedResourceAllocator;

  // Used only at startup
  ResourceAllocationContext immutableAllocationContext;

  // Used during runtime
  ResourceAllocationContext mutableAllocationContext;

  void Build();
  Descriptors(GraphicsDevice &, StartUpSettings &);
  // Can not be moved, this value is pinned after creation!
  Descriptors(const Descriptors &) = delete;
  Descriptors(Descriptors &&) = delete;
};
struct Textures {
  CubeMapTexture skyboxTexture;
  static Textures Default(Descriptors &descriptors);
};
struct Meshes {
  ImmutableMesh planeMesh;
  ImmutableMesh simplePlane;

  ImmutableMesh deferredShadingPlane;
  ImmutableMesh skyboxMesh;
  ImmutableMesh Box;
  ImmutableMesh BoxWithoutBottom;
  /*ImmutableMesh BoxOnlyWithIndexBuffer
;
   ImmutableMesh BoxWithoutBottom
;*/
  static Meshes Default(Descriptors &descriptors);
};

struct App {

  struct RuntimeCPUBuffers {
    std::vector<Graphics::TesselationGraphicRootDescription::OceanData>
        oceanData;
  };

  explicit App(AppShared &shared);

  ~App() = default;

  static void DeleteApp(std::unique_ptr<App> &app);
  void Suspend();
  void Resume();
  void WaitForShutDown();
  void StartRun();
  bool ShouldRestart() const;

  void SetWindow();
  void KeyDown(CoreWindow const &, KeyEventArgs const &args);
  void KeyUp(CoreWindow const &, KeyEventArgs const &args);

  void MouseMoved(CoreWindow const &, PointerEventArgs const &args);
  void MouseWheel(CoreWindow const &, PointerEventArgs const &args);

private:
  void Run();
  void DrawImGuiMenu(CommandAllocator &allocator,
                     Graphics::FrameResources &frameResource,
                     SimulationStage::SimulationResources &drawingSimResource);
  void DrawImGuiApplicationData(
      CommandAllocator &allocator, Graphics::FrameResources &frameResource,
      SimulationStage::SimulationResources &drawingSimResource);
  Graphics::GlobalGPUBuffers
  CreateGlobalGPUBuffers(DynamicBufferManager &bufferManager);

  void CalculateTimeConstants();

  /// From Outer AppWrapper
  AppShared &shared_;

  // DirectX common values
  // -----------------
  GraphicsDevice device;
  CommandQueue directQueue{device};
  // TODO: why does this not work? CommandQueue computeQueue{device};
  CommandQueue &computeQueue = directQueue;
  CoreSwapChain swapChain;

  // Full resource allocation contexts.
  Descriptors descriptors_{device, shared_.settings};

  Textures textures_;
  Meshes meshes_;

  SimulationStage::WaterSimulationPipelines fullSimPipeline =
      SimulationStage::WaterSimulationPipelines::Create(
          device, descriptors_.pipelineStateProvider_);

  Graphics::WaterRenderPipelines fullRenderPipeline =
      Graphics::WaterRenderPipelines::Create(
          device, descriptors_.pipelineStateProvider_,
          Graphics::WaterRenderPipelines::CreateSettings{});

  // Common Data
  Graphics::TesselationGraphicRootDescription::WaterPixelShaderData waterData;
  Graphics::DeferredShading::DeferredShaderBuffers defData;
  Graphics::PixelLighting sunData = Graphics::PixelLighting::SunData();
  SimulationData simData = SimulationData::Default();

  // This is called constant, but they can technically change, since the user is
  // able to overwrite them.
  SimulationStage::ConstantGpuSources<MutableTexture> simulationConstantSources{
      descriptors_.mutableAllocationContext, simData};
  SimulationStage::MutableGpuSources simulationMutableSources{
      descriptors_.mutableAllocationContext, simData};
  // SilhouetteDetector::Buffers silhouetteDetectorBuffers(
  //     mutableAllocationContext, Box.GetIndexCount() * 4);
  // ShadowMapping::Data shadowMapData(cam);

  // Frame data
  // ------------------

  std::vector<std::unique_ptr<Graphics::FrameResources>> frameResources;

  std::vector<std::unique_ptr<SimulationStage::SimulationResources>>
      simulationResources;

  // ImGui Menu and settings
  Menu::ImGUIManager imgui_wrapper_;
  MenuSettings menuSettings_;

  // UWP ApplicationData
  // -----------------
  usize frameCounter_ = 0;
  bool isRunning_ = false;
  bool shouldStop_ = false;
  bool quitRequested_ = false;
  // The app is requesting an internal shutdown and restart
  bool internalRestartRequest_ = false;
  // -----------------

  // AppData
  //---------------

  // For use between all frames
  bool first_loop = true;
  Camera cam = Camera();
  RuntimeSettings settings = RuntimeSettings{};
  DebugValues debugValues = DebugValues{};
  // reuse buffers.
  RuntimeCPUBuffers cpuBuffers;

  // Timing
  float gameTime = 0.f;
  using SinceTimeStartTimeFrame = std::chrono::nanoseconds;
  using TimePoint = decltype(std::chrono::steady_clock::now());
  TimePoint loopStartTime;
  TimePoint currentFrameStart;

  SinceTimeStartTimeFrame GetTimeSinceStart();
  TimeData timeConstants_;

  // Per Frame data
  NeedToDo beforeNextFrame_ = NeedToDo::WithFirstLoopUpdateSettings();

  RuntimeResults runtimeResults_;

  // OceanData
  XMMATRIX
  oceanModelMatrix = XMMatrixTranslationFromVector(XMVECTOR{0, -5, 0, 0});
};
} // namespace Reun
