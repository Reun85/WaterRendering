#include "pch.h"
#include "Simulation.h"
#include "Helpers.h"

void SimulationData::DrawImGui(NeedToDo &out, bool exclusiveWindow) {
  bool cont = true;
  if (exclusiveWindow) {
    cont = ImGui::Begin("Simulation Data");
  }
  if (cont) {

    static const std::vector<std::pair<std::string, SimulationData>> presets =
        SimulationData::Presets();

    static int selectedPreset = 0;

    bool change = false;
    ImGui::Text("N: %d, M: %d", N, M);
    if (ImGui::BeginCombo("Presets", presets[selectedPreset].first.c_str())) {
      for (int i = 0; i < presets.size(); i++) {
        bool isSelected = selectedPreset == i;
        if (ImGui::Selectable(presets[i].first.c_str(), isSelected)) {

          selectedPreset = i;
          change = true;
          *this = presets[i].second;
        }
        if (isSelected)
          ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }
    change |= ImGui::InputFloat2("Wind Direction", &windDirection.x);
    change |= ImGui::InputFloat("Gravity", &gravity);
    change |= ImGui::InputFloat("Depth", &Depth);
    ImGui::Text("Highest");
    bool hig = Highest.DrawImGui("Highest");
    ImGui::Separator();
    ImGui::Text("Medium");
    bool med = Medium.DrawImGui("Medium");
    ImGui::Separator();
    ImGui::Text("Lowest");
    bool low = Lowest.DrawImGui("Lowest");
    ImGui::Separator();
    change |= ImGui::InputFloat("QuadTree distanceThreshold",
                                &quadTreeDistanceThreshold);
    ImGui::Separator();
    out.patchHighestChanged = hig || change;
    out.patchMediumChanged = med || change;
    out.patchLowestChanged = low || change;
  }
  if (exclusiveWindow)
    ImGui::End();
}
bool SimulationData::PatchData::DrawImGui(std::string_view ID) {
  bool change = false;
  const std::string text1 = "Patch Size##" + std::string(ID);
  change |= ImGui::InputFloat(text1.c_str(), &patchSize);
  const std::string text2 = "Foam Decay##" + std::string(ID);
  change |= ImGui::SliderFloat(text2.c_str(), &foamExponentialDecay, 0, 1);
  const std::string text3 = "Displacement Lambda##" + std::string(ID);
  change |= ImGui::InputFloat3(text3.c_str(), (float *)&displacementLambda);
  const std::string text4 = "Amplitude##" + std::string(ID);
  change |= ImGui::InputFloat(text4.c_str(), &Amplitude, 0, 0, "%.5f");
  const std::string text5 = "WindForce##" + std::string(ID);
  change |= ImGui::InputFloat(text5.c_str(), &WindForce);
  const std::string text6 = "Foam Min Value##" + std::string(ID);
  change |= ImGui::InputFloat(text6.c_str(), &foamMinValue);
  const std::string text7 = "Foam Bias##" + std::string(ID);
  change |= ImGui::InputFloat(text7.c_str(), &foamBias);
  const std::string text8 = "Foam Mult##" + std::string(ID);
  change |= ImGui::InputFloat(text8.c_str(), &foamMult);
  return change;
}

static SimulationData Preset1() {

  const auto &N = DefaultsValues::Simulation::N;
  const auto &M = DefaultsValues::Simulation::N;

  SimulationData res{.N = N,
                     .M = M,
                     .windDirection = float2(-1, 1),
                     .gravity = 9.81f,
                     .Depth = 100.f,
                     .Highest =
                         {
                             .displacementLambda = float3(1.f, 0.9f, 1.0f),
                             .patchSize = 5.f,
                             .foamExponentialDecay = 0.320f,
                             .Amplitude = 0.015f,
                             .WindForce = 9,
                             .foamMinValue = 0.4f,
                             .foamBias = 0.2f,
                             .foamMult = 1,
                             .N = res.N,
                             .M = res.M,
                             .windDirection = res.windDirection,
                             .gravity = res.gravity,
                             .Depth = res.Depth,
                         },
                     .Medium =
                         {
                             .displacementLambda = float3(1.0f, 1.3f, 1.0f),
                             .patchSize = 91.f,
                             .foamExponentialDecay = 0.17f,
                             .Amplitude = 0.00024f,
                             .WindForce = 1,
                             .foamMinValue = 0.4f,
                             .foamBias = 0.2f,
                             .foamMult = 1,
                             .N = res.N,
                             .M = res.M,
                             .windDirection = res.windDirection,
                             .gravity = res.gravity,
                             .Depth = res.Depth,
                         },
                     .Lowest = {
                         .displacementLambda = float3(0.7f, 0.7f, 0.7f),
                         .patchSize = 383,
                         .foamExponentialDecay = 0.023f,
                         .Amplitude = 0.00001f,
                         .WindForce = 6,
                         .foamMinValue = 0.4f,
                         .foamBias = 1.0f,
                         .foamMult = 1,
                         .N = res.N,
                         .M = res.M,
                         .windDirection = res.windDirection,
                         .gravity = res.gravity,
                         .Depth = res.Depth,
                     }};
  return res;
}

SimulationData OldPreset() {
  const auto &N = DefaultsValues::Simulation::N;
  const auto &M = DefaultsValues::Simulation::N;
  const auto wind = float2(-1.f, 1.0f);
  const auto gravity = 9.81f;
  const f32 Depth = 100.f;
  SimulationData res{.N = N,
                     .M = M,
                     .windDirection = wind,
                     .gravity = gravity,
                     .Depth = Depth,
                     .Highest =
                         {
                             .displacementLambda = float3(0.48f, 0.5, 0.48f),
                             .patchSize = 21.f,
                             .foamExponentialDecay = 0.1f,
                             .Amplitude = 0.4e-3f,
                             .WindForce = 3,
                             .foamMinValue = 0.4f,
                             .foamBias = 0.2f,
                             .foamMult = 1,
                             .N = res.N,
                             .M = res.M,
                             .windDirection = res.windDirection,
                             .gravity = res.gravity,
                             .Depth = res.Depth,
                         },
                     .Medium =
                         {
                             .displacementLambda = float3(0.48f, 0.5, 0.48f),
                             .patchSize = 91.f,
                             .foamExponentialDecay = 0.1f,
                             .Amplitude = 0.15e-3f,
                             .WindForce = 3,
                             .foamMinValue = 0.4f,
                             .foamBias = 0.2f,
                             .foamMult = 1,
                             .N = res.N,
                             .M = res.M,
                             .windDirection = res.windDirection,
                             .gravity = res.gravity,
                             .Depth = res.Depth,
                         },
                     .Lowest = {
                         .displacementLambda = float3(0.48f, 0.5, 0.48f),
                         .patchSize = 383,
                         .foamExponentialDecay = 0.1f,
                         .Amplitude = 0.1e-4f,
                         .WindForce = 6,
                         .foamMinValue = 0.4f,
                         .foamBias = 0.2f,
                         .foamMult = 1,
                         .N = res.N,
                         .M = res.M,
                         .windDirection = res.windDirection,
                         .gravity = res.gravity,
                         .Depth = res.Depth,
                     }};

  return res;
}
std::vector<std::pair<string, SimulationData>> SimulationData::Presets() {

  std::vector<std::pair<string, SimulationData>> res;
  res.emplace_back("Large", SimulationData::Default());
  res.emplace_back("Medium", Preset1());
  res.emplace_back("Weak", OldPreset());
  return res;
}

SimulationData SimulationData::Default() {
  const auto &N = DefaultsValues::Simulation::N;
  const auto &M = DefaultsValues::Simulation::N;

  const auto wind = float2(-1.f, 1.0f);
  const auto gravity = 9.81f;
  const f32 Depth = 100.f;
  SimulationData res{.N = N,
                     .M = M,
                     .windDirection = wind,
                     .gravity = gravity,
                     .Depth = Depth,
                     .Highest =
                         {
                             .displacementLambda = float3(1.f, 0.9f, 1.0f),
                             .patchSize = 5.f,
                             .foamExponentialDecay = 0.320f,
                             .Amplitude = 0.005f,
                             .WindForce = 3,
                             .foamMinValue = 0.4f,
                             .foamBias = 0.2f,
                             .foamMult = 1,
                             .N = res.N,
                             .M = res.M,
                             .windDirection = res.windDirection,
                             .gravity = res.gravity,
                             .Depth = res.Depth,
                         },
                     .Medium =
                         {
                             .displacementLambda = float3(1.0f, 1.3f, 1.0f),
                             .patchSize = 91.f,
                             .foamExponentialDecay = 0.17f,
                             .Amplitude = 0.00024f,
                             .WindForce = 1,
                             .foamMinValue = 0.4f,
                             .foamBias = 0.2f,
                             .foamMult = 1,
                             .N = res.N,
                             .M = res.M,
                             .windDirection = res.windDirection,
                             .gravity = res.gravity,
                             .Depth = res.Depth,
                         },
                     .Lowest = {
                         .displacementLambda = float3(1.0f, 1.0f, 1.0f),
                         .patchSize = 383,
                         .foamExponentialDecay = 0.023f,
                         .Amplitude = 0.00001f,
                         .WindForce = 6,
                         .foamMinValue = 0.4f,
                         .foamBias = -0.4f,
                         .foamMult = 1,
                         .N = res.N,
                         .M = res.M,
                         .windDirection = res.windDirection,
                         .gravity = res.gravity,
                         .Depth = res.Depth,
                     }};

  return res;
}
