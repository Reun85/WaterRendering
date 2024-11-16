
#pragma once

#include "pch.h"
#include "Defaults.h"
#include "Helpers.h"
#include "Simulation.h"

using namespace std;
using namespace winrt;

using namespace Windows;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::Foundation::Numerics;
using namespace Windows::UI;
using namespace Windows::UI::Core;
using namespace Windows::UI::Composition;

using namespace Axodox::Graphics::D3D12;
using namespace Axodox::Infrastructure;
using namespace Axodox::Storage;
using namespace Axodox::Threading;
using namespace DirectX;
using namespace DirectX::PackedVector;

struct DebugValues {
  enum class Mode : u8 {
    Full = 0,
    DisplacementHighest,
    DisplacementMedium,
    DisplacementLowest,
    GradientsHighest,
    GradientsMedium,
    GradientsLowest,
  };
  enum class DrawMethod : u8 { Tesselation, Parallax, PrismParallax };

  XMFLOAT4 pixelMult = XMFLOAT4(1, 1, 1, 1);
  XMUINT4 swizzleorder = XMUINT4(0, 1, 2, 3);
  XMFLOAT4 blendDistances = XMFLOAT4(150.f, 300, 1000, 5000);
  XMFLOAT3 foamColor = XMFLOAT3(1, 1, 1);

  std::array<bool, 31 - 0 + 1> DebugBits{false, false, true,
                                         true,  false, false};

  bool enableSSR = false;
  bool lockQuadTree = false;
  int maxConeStep = 40;
  float prismHeight = 2;
  DrawMethod drawMethod = DrawMethod::PrismParallax;
  const static constexpr std::initializer_list<
      std::pair<u8, std::optional<const char *>>>
      DebugBitsDesc = {{2, "Use Foam"},
                       {3, "Use channel Highest"},
                       {4, "Use channel Medium"},
                       {5, "Use channel Lowest"},
                       {8, "Show albedo"},
                       {9, "Show normal"},
                       {10, "Show neg normal"},
                       {11, "Show worldPos"},
                       {12, "Show materialValues"},
                       {13, "Show depth"},
                       {24, "Normal overflow"},
                       {25, "Display Foam"},

                       {20, "F"},
                       {21, "D"},
                       {22, "G"},

                       {26, std::nullopt},
                       {27, std::nullopt},
                       {28, std::nullopt},
                       {29, std::nullopt},
                       {30, std::nullopt},
                       {31, std::nullopt}};

  Mode mode = Mode::Full;

  bool calculateParallax() const {
    return drawMethod == DrawMethod::Parallax ||
           drawMethod == DrawMethod::PrismParallax;
  }
  std::array<bool, 3> getChannels() {
    // I am sorry, I am lazy
    return {DebugBits[3], DebugBits[4], DebugBits[5]};
  }
  RasterizerFlags rasterizerFlags = RasterizerFlags::CullClockwise;
  void DrawImGui(NeedToDo &out, bool exclusiveWindow = true) {
    bool cont = true;
    if (exclusiveWindow)
      cont = ImGui::Begin("Debug Values");
    if (cont) {
      // useTexture
      useTextureImGuiDraw();
      // Culling
      CullingImGuiDraw(out);

      ImGui::InputFloat4("Blend Distances", (float *)&blendDistances);
      ImGui::InputFloat("Prism Height", &prismHeight);
      ImGui::Checkbox("Enable SSR", &enableSSR);
      ImGui::Checkbox("Lock QuadTree", &lockQuadTree);

      static const std::array<std::string, 3> modeitems = {
          "Tesselation",
          "Parallax",
          "PrismParallax",
      };
      if (ImGui::BeginCombo("Draw Mode",
                            modeitems[(u32)(drawMethod)].c_str())) {
        for (uint i = 0; i < modeitems.size(); i++) {
          bool isSelected = ((u32)(drawMethod) == i);
          if (ImGui::Selectable(modeitems[i].c_str(), isSelected)) {
            drawMethod = DrawMethod(i);
          }
          if (isSelected) {
            ImGui::SetItemDefaultFocus();
          }
        }
        ImGui::EndCombo();
      }

      ImGui::InputInt("Max Cone Step", &maxConeStep);
      for (auto &[id, name] : DebugBitsDesc) {
        if (name)
          ImGui::Checkbox(*name, &DebugBits[id]);
        else
          ImGui::Checkbox(std::format("Debug Bit {}", id).c_str(),
                          &DebugBits[id]);
      }
    }
    if (exclusiveWindow)
      ImGui::End();
  }

private:
  void CullingImGuiDraw(NeedToDo &out) {
    {
      static const std::array<std::string, 3> items = {"CullClockwise",
                                                       "CullNone", "WireFrame"};
      static uint itemsIndex =
          rasterizerFlags == RasterizerFlags::CullNone
              ? 1
              : (rasterizerFlags == RasterizerFlags::CullClockwise ? 0 : 2);

      if (ImGui::BeginCombo("Render State", items[itemsIndex].c_str())) {
        for (uint i = 0; i < items.size(); i++) {
          bool isSelected = (itemsIndex == i);
          if (ImGui::Selectable(items[i].c_str(), isSelected)) {
            itemsIndex = i;
            switch (itemsIndex) {
              using enum Axodox::Graphics::D3D12::RasterizerFlags;
            case 1:
              rasterizerFlags = CullNone;
              break;
            case 2:
              rasterizerFlags = Wireframe;
              break;
            default:
              rasterizerFlags = CullClockwise;
              break;
            }
            out.changeFlag = rasterizerFlags;
          }
          if (isSelected) {
            ImGui::SetItemDefaultFocus();
          }
        }
        ImGui::EndCombo();
      }
    }
  }

private:
  void useTextureImGuiDraw() {
    {
      static const std::array<std::string, 7> modeitems = {
          "Full",
          "DisplacementHighest",
          "DisplacementMedium",
          "DisplacementLowest",
          "GradientsHighest",
          "GradientsMedium",
          "GradientsLowest"};
      if (ImGui::BeginCombo("Mode", modeitems[(u32)(mode)].c_str())) {
        for (uint i = 0; i < modeitems.size(); i++) {
          bool isSelected = ((u32)(mode) == i);
          if (ImGui::Selectable(modeitems[i].c_str(), isSelected)) {
            mode = Mode(i);
            swizzleorder = XMUINT4(0, 1, 2, 3);
          }
          if (isSelected) {
            ImGui::SetItemDefaultFocus();
          }
        }
        ImGui::EndCombo();
      }
      if (mode != Mode::Full) {
        ImGui::InputFloat4("Pixel Mult", (float *)&pixelMult);

        static const std::array<std::string, 4> swizzleitems = {"r", "g", "b",
                                                                "a"};

        const std::array<u32 *const, 4> vals = {
            &swizzleorder.x, &swizzleorder.y, &swizzleorder.z, &swizzleorder.w};
        ImGui::Text("Swizzle");
        for (int i = 0; i < 4; i++) {
          const string id = "##Swizzle" + swizzleitems[i];
          ImGui::Text("%s", swizzleitems[i].c_str());
          ImGui::SameLine();

          if (ImGui::BeginCombo(id.c_str(), swizzleitems[*vals[i]].c_str())) {
            for (size_t j = 0; j < swizzleitems.size(); j++) {
              bool isSelected = (*(vals[i]) == j);
              if (ImGui::Selectable(swizzleitems[j].c_str(), isSelected)) {
                *vals[i] = (u32)j;
              }
              if (isSelected) {
                ImGui::SetItemDefaultFocus();
              }
            }
            ImGui::EndCombo();
          }
        }
      }
    }
  }
};

struct DebugGPUBufferStuff {
  XMFLOAT4 pixelMult;
  XMFLOAT4 blendDistances;
  XMUINT4 swizzleOrder;
  XMFLOAT4 foamInfo;
  XMFLOAT3 patchSizes;
  // 0: use displacement
  // 1: use normal
  // 2: use foam
  // 3: use channel1
  // 4: use channel2
  // 5: use channel3
  // 6: display texture instead of shader
  // 7: transform texture values from [-1,1] to [0,1]
  u32 flags = 0; // Its here because padding
  float EnvMapMult;
  int maxConeStep;
};
static void set_flag(u32 &flag, u32 flagIndex, bool flagValue = true) {
  if (flagValue) {
    flag |= 1 << flagIndex;
  } else {
    flag &= ~(1 << flagIndex);
  }
}
static DebugGPUBufferStuff From(const DebugValues &deb,
                                const SimulationData &simData) {
  DebugGPUBufferStuff res;
  res.pixelMult = XMFLOAT4(deb.pixelMult.x, deb.pixelMult.y, deb.pixelMult.z,
                           deb.pixelMult.w);
  res.swizzleOrder = deb.swizzleorder;
  res.blendDistances = deb.blendDistances;
  res.patchSizes = XMFLOAT3(simData.Highest.patchSize, simData.Medium.patchSize,
                            simData.Lowest.patchSize);
  res.maxConeStep = deb.maxConeStep;

  for (int i = 0; i < deb.DebugBits.size(); i++) {
    set_flag(res.flags, i, deb.DebugBits[i]);
  }

  set_flag(res.flags, 6, true);
  set_flag(res.flags, 0, false);

  switch (deb.mode) {
  case DebugValues::Mode::Full:
    set_flag(res.flags, 0, true);
    set_flag(res.flags, 6, false);
    break;
  case DebugValues::Mode::GradientsHighest:
    set_flag(res.flags, 0, true);
    set_flag(res.flags, 7, true);
    set_flag(res.flags, 6, false);
    set_flag(res.flags, 3, true);
    set_flag(res.flags, 4, false);
    set_flag(res.flags, 5, false);
    break;
  case DebugValues::Mode::GradientsMedium:
    set_flag(res.flags, 0, true);
    set_flag(res.flags, 7, true);
    set_flag(res.flags, 6, false);
    set_flag(res.flags, 3, false);
    set_flag(res.flags, 4, true);
    set_flag(res.flags, 5, false);
    break;
  case DebugValues::Mode::GradientsLowest:
    set_flag(res.flags, 0, true);
    set_flag(res.flags, 7, true);
    set_flag(res.flags, 6, false);
    set_flag(res.flags, 3, false);
    set_flag(res.flags, 4, false);
    set_flag(res.flags, 5, true);
    break;
  default:
    break;
  }
  return res;
}
