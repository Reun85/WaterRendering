#pragma once
#include "pch.h"

namespace Reun {
struct MenuSettings {
  enum Menu {
    App = 0,

    Save = 1,
    SimData = 2,
    PS = 3,
    Debug = 4,
  };
  struct IsDetached {
    struct PerChar {
      bool App = false;
      bool Save = false;
      bool SimData = false;
      bool PS = false;
      bool Debug = false;
    };
    union {
      PerChar per;
      std::array<bool, 5> arr;
    } values;
    bool set(const Menu ind, const bool val = true) {
      values.arr[(usize)ind] = val;
    }
    bool state(const Menu ind) { return values.arr[(usize)ind]; }
  };
  MenuSettings(std::unordered_map<std::string, std::string> &persistence);

  std::vector<bool> isDetached;
  Menu SelectedMenu = Menu::App;
  bool showDebugMenus = false;

private:
  // Will write to here at destructor
  std::unordered_map<std::string, std::string> &persistence;
};
} // namespace Reun