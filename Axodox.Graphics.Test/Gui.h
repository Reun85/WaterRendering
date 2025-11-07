#pragma once
#include "pch.h"

namespace Reun {
struct MenuSettings {
  struct PanelState {
    std::function<void()> drawContent;
    bool isDetached = false;

    std::string name;
    // Unique name for ImGui
    std::string safeName;
    std::string detachButton;
    std::string attachButton;
    explicit PanelState(std::string name);
    PanelState(std::string name, std::string safe_name);
  };

  static constexpr usize s_menuCount = 5;

  MenuSettings(std::unordered_map<std::string, std::string> &persistence);
  ~MenuSettings();
  void Draw();

  std::string safeName;
  std::string safeBarName;
  PanelState app{"App"};
  PanelState save{"Save"};
  PanelState simData{"SimData"};
  PanelState renderingData{"Rendering data"};
  PanelState debugMenu{"Debug Menu"};

private:
  const std::array<MenuSettings::PanelState *, s_menuCount> list = {
      &app, &save, &simData, &renderingData, &debugMenu};
  // Will write to here at destructor
  std::unordered_map<std::string, std::string> &persistence;
};
} // namespace Reun