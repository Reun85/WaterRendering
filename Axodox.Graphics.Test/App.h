#pragma once
#include "pch.h"
#include <pix3.h>
#include <iostream>
#include <sstream>
#include <string.h>

#include "Camera.h"
#include "ComputePipeline.h"
#include "DebugValues.h"
#include "Defaults.h"
#include "GraphicsPipeline.h"
#include "Helpers.h"
#include "Parallax.h"
#include "QuadTree.h"
#include "ShadowVolume.h"
#include "Simulation.h"
#include "SkyboxPipeline.hpp"
#include "TestConfigLoader.h"

#include "AppShared.h"
#include "ImGuiHelper.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Storage;
using namespace Axodox::Threading;

struct TimeData {
  float deltaTime;
  float timeSinceLaunch;
};

struct App {

  struct RuntimeCPUBuffers {
    std::vector<WaterGraphicRootDescription::OceanData> oceanData;
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
                     WaterGraphicRootDescription::WaterPixelShaderData &,
                     SimulationData &, DeferredShading::DeferredShaderBuffers &,
                     SimulationStage::SimulationResources &, PixelLighting &);

  /// From Outer AppWrapper
  AppShared &shared_;

  // DirectX
  // -----------------
  GraphicsDevice device;
  CommandQueue directQueue{device};
  // TODO: why does this not work? CommandQueue computeQueue{device};
  CommandQueue &computeQueue = directQueue;
  CoreSwapChain swapChain;

  ImGUIManager imgui_wrapper_;

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

  // Timing
  float gameTime = 0.f;
  using SinceTimeStartTimeFrame = std::chrono::nanoseconds;
  decltype(std::chrono::high_resolution_clock::now()) loopStartTime;
  SinceTimeStartTimeFrame GetTimeSinceStart();

  // Camera
  Camera cam = Camera();
  RuntimeSettings settings = RuntimeSettings{};
  DebugValues debugValues = DebugValues{};

  // Per Frame data
  NeedToDo beforeNextFrame_;
  RuntimeResults runtimeResults_;
};
