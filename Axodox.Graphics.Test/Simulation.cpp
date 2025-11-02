#include "pch.h"
#include "Simulation.h"

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
    change |= ImGui::InputFloat("Depth", &depth);
    ImGui::Text("Highest");
    bool hig = highest.DrawImGui("Highest");
    ImGui::Separator();
    ImGui::Text("Medium");
    bool med = medium.DrawImGui("Medium");
    ImGui::Separator();
    ImGui::Text("Lowest");
    bool low = lowest.DrawImGui("Lowest");
    ImGui::Separator();
    ImGui::InputFloat("QuadTree distanceThreshold", &quadTreeDistanceThreshold);
    i32 x = (i32)maxDepth;
    ImGui::InputInt("Max Depth", &x);
    maxDepth = (u32)x;
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
  const std::string text1_2 = "Patch Display Size##" + std::string(ID);
  change |= ImGui::InputFloat(text1_2.c_str(), &patchExtent);
  const std::string text2 = "Foam Decay##" + std::string(ID);
  change |= ImGui::SliderFloat(text2.c_str(), &foamExponentialDecay, 0, 1);
  const std::string text3 = "Displacement Lambda##" + std::string(ID);
  change |= ImGui::InputFloat3(text3.c_str(), (float *)&displacementLambda);
  const std::string text4 = "Amplitude##" + std::string(ID);
  change |= ImGui::InputFloat(text4.c_str(), &amplitude, 0, 0, "%.5f");
  const std::string text5 = "WindForce##" + std::string(ID);
  change |= ImGui::InputFloat(text5.c_str(), &windForce);
  const std::string text6 = "Foam Min Value##" + std::string(ID);
  change |= ImGui::InputFloat(text6.c_str(), &foamMinValue);
  const std::string text7 = "Foam Bias##" + std::string(ID);
  change |= ImGui::InputFloat(text7.c_str(), &foamBias);
  const std::string text8 = "Foam Mult##" + std::string(ID);
  change |= ImGui::InputFloat(text8.c_str(), &foamMult);
  return change;
}

bool SimulationData::PatchData::compatibleSim(const PatchData &other) const {
  return N == other.N && M == other.M && windDirection == other.windDirection &&
         gravity == other.gravity && depth == other.depth &&
         patchSize == other.patchSize && amplitude == other.amplitude &&
         windForce == other.windForce;
}

static SimulationData Preset1() {
  const auto &N = DefaultsValues::Simulation::N;
  const auto &M = DefaultsValues::Simulation::N;

  SimulationData res{.N = N,
                     .M = M,
                     .windDirection = float2(-1, 1),
                     .gravity = 9.81f,
                     .depth = 100.f,
                     .highest =
                         {
                             .displacementLambda = float3(0.f, 0.9f, 0.0f),
                             .patchExtent = 5.f,

                             .patchSize = 5.f,
                             .foamExponentialDecay = 0.320f,
                             .amplitude = 0.009f,
                             .windForce = 9,
                             .foamMinValue = 0.4f,
                             .foamBias = 0.2f,
                             .foamMult = 1,
                             .N = res.N,
                             .M = res.M,
                             .windDirection = res.windDirection,
                             .gravity = res.gravity,
                             .depth = res.depth,
                         },
                     .medium =
                         {
                             .displacementLambda = float3(0.0f, 1.3f, 0.0f),
                             .patchExtent = 91.f,
                             .patchSize = 91.f,
                             .foamExponentialDecay = 0.17f,
                             .amplitude = 0.00001f,
                             .windForce = 9,
                             .foamMinValue = 0.4f,
                             .foamBias = 0.2f,
                             .foamMult = 1,
                             .N = res.N,
                             .M = res.M,
                             .windDirection = res.windDirection,
                             .gravity = res.gravity,
                             .depth = res.depth,
                         },
                     .lowest = {
                         .displacementLambda = float3(0.0f, 0.7f, 0.0f),
                         .patchExtent = 383.f,
                         .patchSize = 383,
                         .foamExponentialDecay = 0.023f,
                         .amplitude = 0.00001f,
                         .windForce = 6,
                         .foamMinValue = 0.4f,
                         .foamBias = -.4f,
                         .foamMult = 1.f,
                         .N = res.N,
                         .M = res.M,
                         .windDirection = res.windDirection,
                         .gravity = res.gravity,
                         .depth = res.depth,
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
                     .depth = Depth,
                     .highest =
                         {
                             .displacementLambda = float3(0.0f, 0.5, 0.0f),
                             .patchExtent = 13.f,
                             .patchSize = 13.f,
                             .foamExponentialDecay = 0.1f,
                             .amplitude = 0.4e-3f,
                             .windForce = 3,
                             .foamMinValue = 0.4f,
                             .foamBias = 0.2f,
                             .foamMult = 1,
                             .N = res.N,
                             .M = res.M,
                             .windDirection = res.windDirection,
                             .gravity = res.gravity,
                             .depth = res.depth,
                         },
                     .medium =
                         {
                             .displacementLambda = float3(0.0f, 0.5, 0.0f),
                             .patchExtent = 91.f,
                             .patchSize = 91.f,
                             .foamExponentialDecay = 0.1f,
                             .amplitude = 0.15e-3f,
                             .windForce = 3,
                             .foamMinValue = 0.4f,
                             .foamBias = 0.2f,
                             .foamMult = 1,
                             .N = res.N,
                             .M = res.M,
                             .windDirection = res.windDirection,
                             .gravity = res.gravity,
                             .depth = res.depth,
                         },
                     .lowest = {
                         .displacementLambda = float3(0.0f, 0.5, 0.0f),
                         .patchExtent = 383.f,
                         .patchSize = 383,
                         .foamExponentialDecay = 0.1f,
                         .amplitude = 0.1e-4f,
                         .windForce = 6,
                         .foamMinValue = 0.4f,
                         .foamBias = 0.2f,
                         .foamMult = 1,
                         .N = res.N,
                         .M = res.M,
                         .windDirection = res.windDirection,
                         .gravity = res.gravity,
                         .depth = res.depth,
                     }};

  return res;
}
std::vector<std::pair<std::string, SimulationData>> SimulationData::Presets() {
  std::vector<std::pair<std::string, SimulationData>> res;
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
                     .depth = Depth,
                     .highest =
                         {
                             .displacementLambda = float3(0.f, 2.0f, 0.0f),
                             .patchExtent = 12.f,
                             .patchSize = 12.f,
                             .foamExponentialDecay = 0.320f,
                             .amplitude = 0.005f,
                             .windForce = 9,
                             .foamMinValue = 0.4f,
                             .foamBias = 0.2f,
                             .foamMult = 1,
                             .N = res.N,
                             .M = res.M,
                             .windDirection = res.windDirection,
                             .gravity = res.gravity,
                             .depth = res.depth,
                         },
                     .medium =
                         {
                             .displacementLambda = float3(0.0f, 1.3f, 0.0f),
                             .patchExtent = 91.f,
                             .patchSize = 91.f,
                             .foamExponentialDecay = 0.17f,
                             .amplitude = 0.00003f,
                             .windForce = 9,
                             .foamMinValue = 0.4f,
                             .foamBias = 0.2f,
                             .foamMult = 1,
                             .N = res.N,
                             .M = res.M,
                             .windDirection = res.windDirection,
                             .gravity = res.gravity,
                             .depth = res.depth,
                         },
                     .lowest = {
                         .displacementLambda = float3(0.0f, 1.0f, 0.0f),
                         .patchExtent = 383.f,
                         .patchSize = 383,
                         .foamExponentialDecay = 0.023f,
                         .amplitude = 0.000002f,
                         .windForce = 15,
                         .foamMinValue = 0.4f,
                         .foamBias = -0.4f,
                         .foamMult = 1,
                         .N = res.N,
                         .M = res.M,
                         .windDirection = res.windDirection,
                         .gravity = res.gravity,
                         .depth = res.depth,
                     }};

  return res;
}