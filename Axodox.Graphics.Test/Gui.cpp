#include "pch.h"
#include "Gui.h"
#include <string_view>
#include <ranges>

namespace Reun {

using namespace std::literals;
static const std::string_view s_detachedValue = "detached"sv;
static const std::string_view s_isDetachedKeyMod = "_is_detached"sv;
static const std::string_view s_active_key = "MOD-Active"sv;
template <typename Map, typename Key>
auto get_optional_from_map(Map &&m, Key &&k) {
  auto it = m.find(std::forward<Key>(k));
  if (it == m.end())
    return std::optional<
        std::reference_wrapper<const std::decay<Map>::type::mapped_type>>{};
  return std::optional<
      std::reference_wrapper<const std::decay<Map>::type::mapped_type>>{
      it->second};
}

MenuSettings::PanelState::PanelState(std::string name)
    : PanelState(name, name + "##x") {}
MenuSettings::PanelState::PanelState(std::string name, std::string safeName)
    : name(name), safeName(safeName), detachButton("Detach##" + safeName),
      attachButton("Re-attach##" + safeName) {}

MenuSettings::MenuSettings(
    std::unordered_map<std::string, std::string> &persistence)
    : persistence(persistence), safeName("##MainAppMenu"),
      safeBarName(safeName + "Bar") {
  // Load!
  auto f = [persistence](PanelState &panel) {
    std::optional<std::reference_wrapper<const std::string>> x =
        get_optional_from_map(persistence,
                              panel.name + std::string(s_isDetachedKeyMod));
    if (x) {
      if (x->get() == s_detachedValue) {
        panel.isDetached = true;
      }
    }
  };
  for (PanelState *p : list) {
    f(*p);
  }
}
MenuSettings::~MenuSettings() {
  // Save

  auto f = [this](PanelState &panel) {
    if (panel.isDetached) {
      persistence[panel.name + std::string(s_isDetachedKeyMod)] = s_detachedValue;
    } else {
      persistence.erase(panel.name + std::string(s_isDetachedKeyMod));
    }
  };
  for (PanelState *p : list) {
    f(*p);
  }
}

void DrawPanelContent(MenuSettings::PanelState &state, const bool isDetached) {
  std::string *but;
  if (isDetached) {
    but = &state.attachButton;
  } else {
    but = &state.detachButton;
  }
  if (ImGui::Button(but->c_str())) {
    state.isDetached = !state.isDetached;
  }
  state.drawContent();
}

void MenuSettings::Draw() {
  bool any_not_detached = false;

  // Floating panels
  for (PanelState *p : list) {
    auto &panel = *p;
    if (panel.isDetached) {

      if (ImGui::Begin(panel.safeName.c_str())) {
        DrawPanelContent(panel, true);
      }
      ImGui::End();
    } else {
      any_not_detached = true;
    }
  }

  if (!any_not_detached)
    return;
  if (ImGui::Begin(safeName.c_str())) {

    if (ImGui::BeginTabBar(safeBarName.c_str())) {

      for (PanelState *p : list) {
        auto &panel = *p;
        if (!panel.isDetached) {

          if (ImGui::BeginTabItem(panel.name.c_str())) {

            DrawPanelContent(panel, false);
            ImGui::EndTabItem();
          }
        }
      }

      ImGui::EndTabBar();
    }
  }
  ImGui::End();
}
} // namespace Reun