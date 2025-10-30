#include "pch.h"
#include "Camera.h"
#include "QuadTree.h"
#include <string.h>
#include "Defaults.h"
#include "Simulation.h"
#include "Helpers.h"
#include "pix3.h"
#include "ComputePipeline.h"
#include "GraphicsPipeline.h"
#include "SkyboxPipeline.hpp"
#include "ShadowVolume.h"
#include "Parallax.h"
#include "DebugValues.h"
#include <TestConfigLoader.h>

#include <sstream>

using namespace Axodox::Infrastructure;
using namespace Axodox::Storage;
using namespace Axodox::Threading;
using namespace DirectX::PackedVector;

struct StartUpSettings {
  u8 framesInFlight = 2;
  std::string ImGuiIniPath = GetLocalFolder() + "/imgui.ini";
};

/// <summary>
///  Mainly used for WinRT objects that need to be accessed from the wrapper or
///  the app itself.
/// Also used for debug printing.
/// </summary>
struct AppShared {
  // WinRT
  CoreWindow window = nullptr;
  CoreDispatcher dispatcher = nullptr;
  // debug purposes
  // uses IMGUI for printing data to the screen instead of console
  std::string prints = std::string();
  // use this instead of std::cout
  std::stringstream cout = std::stringstream();

  StartUpSettings settings;
};
