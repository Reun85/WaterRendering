#include "pch.h"
#include "Simulation.h"
#include "Helpers.h"

void SimulationData::DrawImGui(NeedToDo &out, bool exclusiveWindow) {
  bool cont = true;
  if (exclusiveWindow) {
    cont = ImGui::Begin("Simulation Data");
  }
  if (cont) {
    ImGui::Text("N: %d, M: %d", N, M);
    bool change = false;
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
    out.PatchHighestChanged = hig || change;
    out.PatchMediumChanged = med || change;
    out.PatchLowestChanged = low || change;
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

SimulationData SimulationData::Default() {
  const auto &N = DefaultsValues::Simulation::N;
  const auto &M = DefaultsValues::Simulation::N;
  const auto &wind = DefaultsValues::Simulation::WindDirection;
  const auto &gravity = DefaultsValues::Simulation::gravity;
  const auto &Depth = DefaultsValues::Simulation::Depth;
  SimulationData res{.N = N,
                     .M = M,
                     .windDirection = wind,
                     .gravity = gravity,
                     .Depth = Depth,
                     .Highest =
                         {
                             .displacementLambda = float3(0.48f, 0.5, 0.48f),
                             .patchSize = 21.f,
                             .foamExponentialDecay = 0.3f,
                             .Amplitude = 0.01e-0f,
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
                             .foamExponentialDecay = 0.3f,
                             .Amplitude = 0.35e-3f,
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
                         .foamExponentialDecay = 0.3f,
                         .Amplitude = 0.00003,
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