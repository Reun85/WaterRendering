#pragma once
#include "pch.h"
#include "ComputePipeline.h"
#include "GraphicsPipeline.h"

namespace Reun {

namespace Menu {
using namespace Axodox::Infrastructure;
inline std::optional<usize>
DisplayComboBoxByIndex(const char *label,
                       const std::span<const char *const> &items,
                       const usize &currentIndex, bool display_preview = true) {

  const bool if_not_preview = !display_preview && ImGui::BeginCombo(label, "");
  const bool if_preview =
      display_preview && ImGui::BeginCombo(label, items[currentIndex]);

  std::optional<usize> ret = std::nullopt;
  if (if_not_preview || if_preview) {
    for (usize ind = 0; ind < items.size(); ind++) {
      const auto &name = items[ind];
      bool isSelected = (currentIndex == ind);
      if (ImGui::Selectable(name, isSelected)) {
        ret = ind;
      }
      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
  return ret;
}

inline std::optional<usize>
DisplayComboBoxByIndex(const char *label,
                       const std::span<const std::string> &items,
                       const usize &currentIndex, bool display_preview = true) {

  const bool if_not_preview = !display_preview && ImGui::BeginCombo(label, "");
  const bool if_preview =
      display_preview && ImGui::BeginCombo(label, items[currentIndex].c_str());

  std::optional<usize> ret = std::nullopt;
  if (if_not_preview || if_preview) {
    for (usize ind = 0; ind < items.size(); ind++) {
      const auto &name = items[ind];
      bool isSelected = (currentIndex == ind);
      if (ImGui::Selectable(name.c_str(), isSelected)) {
        ret = ind;
      }
      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
  return ret;
}

template <typename T>
inline std::optional<T>
DisplayComboBox(const char *label,
                const std::span<const std::pair<const char *, T>> &items,
                const T &currentValue, bool display_preview = true) {
  // find elements index
  const auto pos = std::ranges::find_if(
      items, [&](const auto &item) { return item.second == currentValue; });

  const usize currentIndex = std::distance(items.begin(), pos);

  const bool if_not_preview = !display_preview && ImGui::BeginCombo(label, "");
  const bool if_preview =
      display_preview && ImGui::BeginCombo(label, items[currentIndex].first);

  std::optional<usize> ret = std::nullopt;
  if (if_not_preview || if_preview) {
    for (usize ind = 0; ind < items.size(); ind++) {
      const auto &[name, _] = items[ind];
      bool isSelected = (currentIndex == ind);
      if (ImGui::Selectable(name, isSelected)) {
        ret = ind;
      }
      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
  if (ret.has_value()) {
    return items[ret.value()].second;
  } else {
    return std::nullopt;
  }
}

namespace Bess::Config {

/*
Taken from: https://github.com/shivang51/bess/tree/main
MIT license
*/

ImVec4 BlendColors(const ImVec4 &base, const ImVec4 &accent, float blendFactor);

void setBessDarkColors();
} // namespace Bess::Config

struct ImGUIManager {

  ImGUIManager(const Axodox::Graphics::D3D12::GraphicsDevice &device,
               u8 framesInFlight, const std::filesystem::path &iniPath,
               const std::string iniName = "MyApp");

  ImGuiIO &GetIO();
  ~ImGUIManager();

  ID3D12DescriptorHeap *GetHeap();
  void Pre(CommandAllocator &allocator) const;
  void Render(CommandAllocator &allocator) const;
  ImGUIManager(const ImGUIManager &) = delete;
  ImGUIManager(const ImGUIManager &&) = delete;

public:
  /// <summary>
  ///  Keys cannot contain '=', '[]' and the values cannot contain line breaks;
  ///  UB otherwise. Data is saved on destructor.
  /// </summary>
  std::unordered_map<std::string, std::string> settings;

private:
  std::string iniName;
  void SetupPersistence();
  winrt::com_ptr<ID3D12DescriptorHeap> descriptorHeap_ = nullptr;
};
} // namespace Menu
} // namespace Reun
