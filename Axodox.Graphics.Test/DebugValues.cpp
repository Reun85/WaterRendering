#pragma once
#include "pch.h"

namespace Reun {
const static constexpr std::initializer_list<
    std::pair<u8, std::optional<const char *>>>
    DebugBitsDesc = {{u8(2), "Use Foam"},
                     {u8(3), "Use channel Highest"},
                     {u8(4), "Use channel Medium"},
                     {u8(5), "Use channel Lowest"},
                     {u8(8), "Show albedo"},
                     {u8(9), "Show normal"},
                     {u8(10), "Show neg normal"},
                     {u8(11), "Show worldPos"},
                     {u8(12), "Show materialValues"},
                     {u8(13), "Show depth"},
                     {u8(14), "Per pixel show miss"},
                     {u8(15), "Per pixel show too low"},
                     {u8(16), "Per pixel show side usage"},
                     {u8(24), "Normal overflow"},
                     {u8(25), "Display Foam"},

                     {u8(20), "F"},
                     {u8(21), "D"},
                     {u8(22), "G"},

                     {u8(26), std::nullopt},
                     {u8(27), std::nullopt},
                     {u8(28), std::nullopt},
                     {u8(29), std::nullopt},
                     {u8(30), std::nullopt},
                     {u8(31), std::nullopt}};

void DebugValues::DrawImGui(NeedToDo &out, bool exclusiveWindow) {
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
    ImGui::SliderFloat("Cone step relax", &coneStepRelax, 0, 3);
    ImGui::Checkbox("Enable SSR", &enableSSR);
    ImGui::Checkbox("Lock QuadTree", &lockQuadTree);
    ImGui::Checkbox("Cone Creater", &conecreater);

    static const std::array<std::string, 3> modeitems = {
        "Tesselation",
        "Parallax",
        "PrismParallax",
    };
    if (ImGui::BeginCombo("Draw Mode", modeitems[(u32)(drawMethod)].c_str())) {
      for (uint i = 0; i < modeitems.size(); i++) {
        bool isSelected = ((u32)(drawMethod) == i);
        if (ImGui::Selectable(modeitems[i].c_str(), isSelected)) {
          drawMethod = DrawTechnology(i);
        }
        if (isSelected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }

    ImGui::InputInt("Max Cone Step", &maxConeStep);
    for (auto &[id, name] : DebugBitsDesc) {
      if (name.has_value())
        ImGui::Checkbox(*name, &DebugBits[id]);
      else
        ImGui::Checkbox(std::format("Debug Bit {}", id).c_str(),
                        &DebugBits[id]);
    }
  }
  if (exclusiveWindow)
    ImGui::End();
}

void DebugValues::CullingImGuiDraw(NeedToDo &out) {

  using enum Axodox::Graphics::D3D12::RasterizerFlags;
  static const std::array<std::pair<const char *, RasterizerFlags>, 3> items = {
      {{"CullClockwise", CullClockwise},
       {"CullNone", CullNone},
       {"WireFrame", Wireframe}}};

  const auto new_val = Menu::DisplayComboBox<RasterizerFlags>(
      "Render State", std::span(items), rasterizerFlags);
  if (new_val.has_value()) {
    out.changeFlag = new_val.value();
  }
}

void DebugValues::useTextureImGuiDraw() {
  {
    {
      // NOTE: order needs to match Mode enum
      static const std::array<const char *, 7> items = {"None",
                                                        "DisplacementHighest",
                                                        "DisplacementMedium",
                                                        "DisplacementLowest",
                                                        "GradientsHighest",
                                                        "GradientsMedium",
                                                        "GradientsLowest"};

      usize index;
      if (debugTextureMode.has_value())
        index = (usize)debugTextureMode.value() + 1;
      else
        index = 0;
      const auto chosen = Menu::DisplayComboBoxByIndex("Debug Texture Mode",
                                                       std::span(items), index);
      if (chosen.has_value()) {
        const auto val = chosen.value();
        if (val == 0) {
          debugTextureMode = std::nullopt;
        } else {
          debugTextureMode = DebugTextureDisplay(val - 1);
        }

        // also reset swizzleorder
        swizzleorder = XMUINT4(0, 1, 2, 3);
      }
    }

    if (debugTextureMode.has_value()) {
      ImGui::InputFloat4("Pixel Mult", (float *)&pixelMult);

      // Also used for ImGui ID, must be 1 len
      static const std::array<const char *, 4> swizzleitems = {"r", "g", "b",
                                                               "a"};

      const std::array<u32 *const, 4> vals = {&swizzleorder.x, &swizzleorder.y,
                                              &swizzleorder.z, &swizzleorder.w};
      ImGui::Text("Swizzle Order");
      std::string buff = "##Swizzle ";
      for (int i = 0; i < 4; i++) {
        buff[buff.size() - 1] = swizzleitems[i][0];
        const std::string &id = buff;
        ImGui::Text("%s", swizzleitems[i]);
        ImGui::SameLine();

        const auto selectedIndex = *vals[i];
        const auto chosen = Menu::DisplayComboBoxByIndex(
            id.c_str(), std::span(swizzleitems), selectedIndex);

        if (chosen.has_value()) {
          *vals[i] = (u32)chosen.value();
        }
      }
    }
  }
}

DebugGPUBufferStuff From(const DebugValues &deb,
                         const SimulationData &simData) {
  DebugGPUBufferStuff res;
  res.pixelMult = XMFLOAT4(deb.pixelMult.x, deb.pixelMult.y, deb.pixelMult.z,
                           deb.pixelMult.w);
  res.swizzleOrder = XMUINT4(deb.swizzleorder.x, deb.swizzleorder.y,
                             deb.swizzleorder.z, deb.swizzleorder.w);
  res.blendDistances = deb.blendDistances;
  res.patchSizes = XMFLOAT3(simData.highest.patchSize, simData.medium.patchSize,
                            simData.lowest.patchSize);
  res.maxConeStep = deb.maxConeStep;
  res.coneStepRelax = deb.coneStepRelax;
  for (int i = 0; i < deb.DebugBits.size(); i++) {
    set_flag(res.flags, i, deb.DebugBits[i]);
  }

  set_flag(res.flags, 6, true);
  set_flag(res.flags, 0, false);

  if (deb.debugTextureMode.has_value()) {
    const auto val = deb.debugTextureMode.value();

    switch (val) {
    case DebugValues::DebugTextureDisplay::GradientsHighest:
      set_flag(res.flags, 0, true);
      set_flag(res.flags, 7, true);
      set_flag(res.flags, 6, false);
      set_flag(res.flags, 3, true);
      set_flag(res.flags, 4, false);
      set_flag(res.flags, 5, false);
      break;
    case DebugValues::DebugTextureDisplay::GradientsMedium:
      set_flag(res.flags, 0, true);
      set_flag(res.flags, 7, true);
      set_flag(res.flags, 6, false);
      set_flag(res.flags, 3, false);
      set_flag(res.flags, 4, true);
      set_flag(res.flags, 5, false);
      break;
    case DebugValues::DebugTextureDisplay::GradientsLowest:
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
  } else {

    set_flag(res.flags, 0, true);
    set_flag(res.flags, 6, false);
  }

  return res;
}

void RuntimeSettings::DrawImGui([[maybe_unused]] NeedToDo &out,
                                bool exclusiveWindow) {
  bool cont = true;
  if (exclusiveWindow)
    cont = ImGui::Begin("Runtime Settings");
  if (cont) {
    ImGui::ColorEdit3("clear color", (float *)&clearColor);
    ImGui::Checkbox("Time running", &timeRunning);
  }
  if (exclusiveWindow)
    ImGui::End();
}
} // namespace Reun
