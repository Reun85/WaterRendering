#include "pch.h"

namespace Reun {
struct StartUpSettings {
  u8 framesInFlight = 2;
  std::filesystem::path ImGuiIniPath = GetLocalFolder() / "imgui.ini";
  std::filesystem::path cacheLocation = GetCacheFolder();
};

/// <summary>
///  Mainly used for WinRT objects that need to be accessed from the wrapper or
///  the app itself.
/// Also used for debug printing.
/// </summary>
struct AppShared {
  // WinRT
  winrt::Windows::UI::Core::CoreWindow window = nullptr;
  winrt::Windows::UI::Core::CoreDispatcher dispatcher = nullptr;

  // debug purposes
  // uses IMGUI for printing data to the screen instead of console
  std::string prints = std::string();
  // use this instead of std::cout
  std::stringstream cout = std::stringstream();

  StartUpSettings settings;
};
} // namespace Reun
