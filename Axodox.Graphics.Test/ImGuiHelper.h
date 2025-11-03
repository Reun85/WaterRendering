#pragma once
#include "pch.h"
#include "ComputePipeline.h"
#include "GraphicsPipeline.h"

using namespace Axodox::Infrastructure;

namespace ImGuiHelpers {
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
} // namespace ImGuiHelpers

struct ImGUIManager {

  ImGUIManager(const Axodox::Graphics::D3D12::GraphicsDevice &device,
               u8 framesInFlight, const std::string &iniPath);

  ImGuiIO &GetIO();
  ~ImGUIManager();

  ID3D12DescriptorHeap *GetHeap();
  void Pre(CommandAllocator &allocator) const;
  void Render(CommandAllocator &allocator) const;

private:
  winrt::com_ptr<ID3D12DescriptorHeap> descriptorHeap_ = nullptr;
};
