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
  return change;
}

SimulationData SimulationData::Default() {
  const auto &N = Defaults::Simulation::N;
  const auto &M = Defaults::Simulation::N;
  const auto &wind = Defaults::Simulation::WindDirection;
  const auto &gravity = Defaults::Simulation::gravity;
  const auto &WindForce1 = Defaults::Simulation::WindForce1;
  const auto &WindForce2 = Defaults::Simulation::WindForce2;
  const auto &WindForce3 = Defaults::Simulation::WindForce3;
  const auto &Amplitude1 = Defaults::Simulation::Amplitude1;
  const auto &Amplitude2 = Defaults::Simulation::Amplitude2;
  const auto &Amplitude3 = Defaults::Simulation::Amplitude3;
  const auto &patchSize1 = Defaults::Simulation::patchSize1;
  const auto &patchSize2 = Defaults::Simulation::patchSize2;
  const auto &patchSize3 = Defaults::Simulation::patchSize3;
  const auto &Depth = Defaults::Simulation::Depth;
  const auto &exponentialDecay = Defaults::Simulation::exponentialDecay;
  const auto &displacementLambda = Defaults::Simulation::displacementLambda;
  SimulationData res{.N = N,
                     .M = M,
                     .windDirection = wind,
                     .gravity = gravity,
                     .Depth = Depth,
                     .Highest =
                         {
                             .displacementLambda = displacementLambda,
                             .patchSize = patchSize1,
                             .foamExponentialDecay = exponentialDecay,
                             .Amplitude = Amplitude1,
                             .WindForce = WindForce1,
                             .N = res.N,
                             .M = res.M,
                             .windDirection = res.windDirection,
                             .gravity = res.gravity,
                             .Depth = res.Depth,
                         },
                     .Medium =
                         {
                             .displacementLambda = displacementLambda,
                             .patchSize = patchSize2,
                             .foamExponentialDecay = exponentialDecay,
                             .Amplitude = Amplitude2,
                             .WindForce = WindForce2,
                             .N = res.N,
                             .M = res.M,
                             .windDirection = res.windDirection,
                             .gravity = res.gravity,
                             .Depth = res.Depth,
                         },
                     .Lowest = {
                         .displacementLambda = displacementLambda,
                         .patchSize = patchSize3,
                         .foamExponentialDecay = exponentialDecay,
                         .Amplitude = Amplitude3,
                         .WindForce = WindForce3,
                         .N = res.N,
                         .M = res.M,
                         .windDirection = res.windDirection,
                         .gravity = res.gravity,
                         .Depth = res.Depth,
                     }};

  return res;
}