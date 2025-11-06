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
  Descriptors(const Descriptors &) = delete;
  Descriptors(Descriptors &&) = delete;
};

struct App {

  struct RuntimeCPUBuffers {
    std::vector<Graphics::WaterGraphicRootDescription::OceanData> oceanData;
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
  void DrawImGuiMenu(CommandAllocator &allocator, Graphics::FrameResources &,
                     SimulationStage::SimulationResources &);
  Graphics::GlobalGPUBuffers
  CreateGlobalGPUBuffers(DynamicBufferManager &bufferManager);

  void CalculateTimeConstants();

  /// From Outer AppWrapper
  AppShared &shared_;

  // DirectX
  // -----------------
  GraphicsDevice device;
  CommandQueue directQueue{device};
  // TODO: why does this not work? CommandQueue computeQueue{device};
  CommandQueue &computeQueue = directQueue;
  CoreSwapChain swapChain;

  Descriptors descriptors_{device, shared_.settings};

  SimulationStage::WaterSimulationPipelines fullSimPipeline =
      SimulationStage::WaterSimulationPipelines::Create(
          device, descriptors_.pipelineStateProvider_);
  Graphics::WaterRenderPipelines fullRenderPipeline =
      Graphics::WaterRenderPipelines::Create(
          device, descriptors_.pipelineStateProvider_,
          Graphics::WaterRenderPipelines::CreateSettings{});

  // Common Data
  Graphics::WaterGraphicRootDescription::WaterPixelShaderData waterData;
  Graphics::DeferredShading::DeferredShaderBuffers defData;
  Graphics::PixelLighting sunData = Graphics::PixelLighting::SunData();
  SimulationData simData = SimulationData::Default();
  // ShadowMapping::Data shadowMapData(cam);

  // ResourceAllocationContext immutableResourceAllocationContext_;
  // ResourceAllocationContext mutableResourceAllocationContext_;
  // WaterRenderPipelines renderStage_;

  Menu::ImGUIManager imgui_wrapper_;

  // ApplicationData
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
  using TimePoint = decltype(std::chrono::high_resolution_clock::now());
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
