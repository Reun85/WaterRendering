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
    change |= ImGui::InputFloat("Wind Force", &WindForce);
    change |= ImGui::InputFloat("Gravity", &gravity);
    change |= ImGui::InputFloat("Amplitude", &Amplitude);
    change |= ImGui::InputFloat("Depth", &Depth);
    ImGui::Text("Highest");
    bool hig = Highest.DrawImGui();
    ImGui::Separator();
    ImGui::Text("Medium");
    bool med = Medium.DrawImGui();
    ImGui::Separator();
    ImGui::Text("Lowest");
    bool low = Lowest.DrawImGui();
    ImGui::Separator();
    out.PatchHighestChanged = hig || change;
    out.PatchMediumChanged = med || change;
    out.PatchLowestChanged = low || change;
    change |= ImGui::InputFloat("QuadTree distanceThreshold",
                                &quadTreeDistanceThreshold);
  }
  if (exclusiveWindow)
    ImGui::End();
}
bool SimulationData::PatchData::DrawImGui() {
  bool change = false;

  change |= ImGui::InputFloat("Patch Size", &patchSize);
  return change;
}

SimulationData SimulationData::Default() {

  const auto &N = Defaults::Simulation::N;
  const auto &M = Defaults::Simulation::N;
  const auto &wind = Defaults::Simulation::WindDirection;
  const auto &gravity = Defaults::Simulation::gravity;
  const auto &WindForce = Defaults::Simulation::WindForce;
  const auto &Amplitude = Defaults::Simulation::Amplitude;
  const auto &patchSize1 = Defaults::Simulation::patchSize1;
  const auto &patchSize2 = Defaults::Simulation::patchSize2;
  const auto &patchSize3 = Defaults::Simulation::patchSize3;
  const auto &Depth = Defaults::Simulation::Depth;
  SimulationData res{.N = N,
                     .M = M,
                     .windDirection = wind,
                     .WindForce = WindForce,
                     .gravity = gravity,
                     .Amplitude = Amplitude,
                     .Depth = Depth,
                     .Highest =
                         {
                             .patchSize = patchSize1,
                             .N = res.N,
                             .M = res.M,
                             .windDirection = res.windDirection,
                             .WindForce = res.WindForce,
                             .gravity = res.gravity,
                             .Amplitude = res.Amplitude,
                             .Depth = res.Depth,
                         },
                     .Medium =
                         {
                             .patchSize = patchSize2,
                             .N = res.N,
                             .M = res.M,
                             .windDirection = res.windDirection,
                             .WindForce = res.WindForce,
                             .gravity = res.gravity,
                             .Amplitude = res.Amplitude,
                             .Depth = res.Depth,
                         },
                     .Lowest = {
                         .patchSize = patchSize3,
                         .N = res.N,
                         .M = res.M,
                         .windDirection = res.windDirection,
                         .WindForce = res.WindForce,
                         .gravity = res.gravity,
                         .Amplitude = res.Amplitude,
                         .Depth = res.Depth,

                     }};

  return res;
}
