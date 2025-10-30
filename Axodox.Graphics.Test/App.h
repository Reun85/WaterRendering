#include "pch.h"
#include "Camera.h"
#include "ComputePipeline.h"
#include "DebugValues.h"
#include "Defaults.h"
#include "GraphicsPipeline.h"
#include "Helpers.h"
#include "Parallax.h"
#include "pix3.h"
#include "QuadTree.h"
#include "ShadowVolume.h"
#include "Simulation.h"
#include "SkyboxPipeline.hpp"
#include <ImGuiWrapper.h>
#include <string.h>
#include <TestConfigLoader.h>

#include <AppShared.h>
#include <iostream>
#include <sstream>

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
  /// From Outer AppWrapper
  AppShared &shared_;

  // DirectX
  // -----------------
  GraphicsDevice device;
  CommandQueue directQueue{device};
  CommandQueue &computeQueue = directQueue;
  CoreSwapChain swapChain;

  ImGUIManager imgui_wrapper_;

  // AppData
  // -----------------
  Camera cam = Camera();
  RuntimeSettings settings = RuntimeSettings{};
  DebugValues debugValues = DebugValues{};
  bool isRunning_ = false;
  bool shouldStop_ = false;
  bool quitRequested_ = false;
  // The app is requesting an internal shutdown and restart
  bool internalRestartRequest_ = false;
  // -----------------
};
