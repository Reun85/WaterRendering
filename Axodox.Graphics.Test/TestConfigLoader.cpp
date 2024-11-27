#include "pch.h"
#include "TestConfigLoader.h"
#include "Simulation.h"
#include "GraphicsPipeline.h"
#include <filesystem>
#include <iostream>
#include <fstream>
#include "DebugValues.h"

#include "Helpers.h"

template <typename T>
concept EnumType = std::is_enum_v<T>;

template <EnumType Enum>
std::ostream &operator<<(std::ostream &os, Enum value) {
  using UnderlyingType = std::underlying_type_t<Enum>;
  os << static_cast<UnderlyingType>(value);
  return os;
}

template <EnumType Enum>
std::istream &operator>>(std::istream &is, Enum &value) {
  using UnderlyingType = std::underlying_type_t<Enum>;
  UnderlyingType temp;
  is >> temp;
  value = static_cast<Enum>(temp); // Cast back to Enum type
  return is;
}

template <typename T>
std::istream &operator>>(std::istream &is, std::vector<T> &v) {
  for (auto &el : v) {
    is >> el;
  }
  return is;
}

template <typename T>
std::ostream &operator<<(std::ostream &os, const std::vector<T> &v) {
  for (auto &el : v) {
    os << el << " ";
  }
  return os;
}

template <typename T, const u32 N>
std::istream &operator>>(std::istream &is, std::array<T, N> &v) {
  for (auto &el : v) {
    is >> el;
  }
  return is;
}

template <typename T, const u32 N>
std::ostream &operator<<(std::ostream &os, const std::array<T, N> &v) {
  for (auto &el : v) {
    os << el << " ";
  }
  return os;
}

std::istream &operator>>(std::istream &is, float2 &x) {
  is >> x.x >> x.y;
  return is;
}

std::ostream &operator<<(std::ostream &os, const float2 &x) {
  os << x.x << " " << x.y;
  return os;
}
std::istream &operator>>(std::istream &is, float3 &x) {
  is >> x.x >> x.y >> x.z;
  return is;
}

std::ostream &operator<<(std::ostream &os, const float3 &x) {
  os << x.x << " " << x.y << " " << x.z;
  return os;
}

std::istream &operator>>(std::istream &is, float4 &x) {
  is >> x.x >> x.y >> x.z >> x.w;
  return is;
}

std::ostream &operator<<(std::ostream &os, const float4 &x) {
  os << x.x << " " << x.y << " " << x.z << " " << x.w;
  return os;
}

std::istream &operator>>(std::istream &is, XMFLOAT3 &x) {
  is >> x.x >> x.y >> x.z;
  return is;
}

std::ostream &operator<<(std::ostream &os, const XMFLOAT3 &x) {
  os << x.x << " " << x.y << " " << x.z;
  return os;
}
std::istream &operator>>(std::istream &is, XMFLOAT4 &x) {
  is >> x.x >> x.y >> x.z >> x.w;
  return is;
}

std::ostream &operator<<(std::ostream &os, const XMFLOAT4 &x) {
  os << x.x << " " << x.y << " " << x.z << " " << x.w;
  return os;
}

std::istream &operator>>(std::istream &is, XMUINT4 &x) {
  is >> x.x >> x.y >> x.z >> x.w;
  return is;
}

std::ostream &operator<<(std::ostream &os, const XMUINT4 &x) {
  os << x.x << " " << x.y << " " << x.z << " " << x.w;
  return os;
}

std::istream &operator>>(std::istream &is, XMVECTOR &y) {
  XMFLOAT4 x;
  XMStoreFloat4(&x, y);
  is >> x.x >> x.y >> x.z >> x.w;
  y = XMLoadFloat4(&x);
  return is;
}

std::ostream &operator<<(std::ostream &os, const XMVECTOR &y) {
  XMFLOAT4 x;
  XMStoreFloat4(&x, y);
  os << x.x << " " << x.y << " " << x.z << " " << x.w;
  return os;
}
template <typename T>
concept OStreamWritable = requires(std::ostream &os, T value) {
  { os << value } -> std::same_as<std::ostream &>;
};

// Concept to check if a type T has a `>>` operator with std::istream
template <typename T>
concept IStreamReadable = requires(std::istream &is, T &value) {
  { is >> value } -> std::same_as<std::istream &>;
};

// Combined concept for both ostream and istream compatibility
template <typename T>
concept Streamable = OStreamWritable<T> && IStreamReadable<T>;

template <Streamable T> void filedo(std::ios *os, T &val, bool write = false) {
  if (write) {
    auto *output = dynamic_cast<std::ofstream *>(os);
    if (output) {
      *output << val << "\n";
    } else {
      throw std::invalid_argument(
          "Invalid stream type: expected ofstream for writing");
    }
  } else {
    auto *input = dynamic_cast<std::ifstream *>(os);
    if (input) {
      *input >> val;
    } else {
      throw std::invalid_argument(
          "Invalid stream type: expected ifstream for reading");
    }
  }
}
static std::vector<std::pair<std::string, std::filesystem::path>> getFiles() {
  std::vector<std::pair<std::string, std::filesystem::path>> files;

  std::filesystem::path dir =
      std::filesystem::path(GetLocalFolder()) / "SimConfig";
  try {
    for (const auto &entry : std::filesystem::directory_iterator(dir)) {
      if (entry.is_regular_file()) {
        files.emplace_back(entry.path().filename().string(), entry.path());
      }
    }
  } catch (std::exception &ex) {
  }
  return files;
}

void inn(std::ios *s, DebugValues &x, bool v) {
  filedo(s, x.pixelMult, v);
  filedo(s, x.swizzleorder, v);
  filedo(s, x.blendDistances, v);
  filedo(s, x.foamColor, v);
  filedo(s, x.DebugBits, v);
  filedo(s, x.enableSSR, v);
  filedo(s, x.lockQuadTree, v);
  filedo(s, x.maxConeStep, v);
  filedo(s, x.prismHeight, v);
  filedo(s, x.coneStepRelax, v);
  filedo(s, x.drawMethod, v);
  filedo(s, x.mode, v);
  filedo(s, x.rasterizerFlags, v);
}

void inn(std::ios *s, SimulationData::PatchData &x, bool v) {

  filedo(s, x.displacementLambda, v);
  filedo(s, x.patchSize, v);
  filedo(s, x.patchExtent, v);
  filedo(s, x.foamExponentialDecay, v);
  filedo(s, x.Amplitude, v);
  filedo(s, x.WindForce, v);
  filedo(s, x.foamMinValue, v);
  filedo(s, x.foamBias, v);
  filedo(s, x.foamMult, v);
  filedo(s, x.N, v);
  filedo(s, x.M, v);
  filedo(s, x.windDirection, v);
  filedo(s, x.gravity, v);
  filedo(s, x.Depth, v);
};

void inn(std::ios *s, SimulationData &x, bool v) {
  inn(s, x.Highest, v);
  inn(s, x.Medium, v);
  inn(s, x.Lowest, v);
  filedo(s, x.N, v);
  filedo(s, x.M, v);
  filedo(s, x.windDirection, v);
  filedo(s, x.gravity, v);
  filedo(s, x.Depth, v);
  filedo(s, x.quadTreeDistanceThreshold, v);
  filedo(s, x.maxDepth, v);
}
void inn(std::ios *s, WaterGraphicRootDescription::WaterPixelShaderData &x,
         bool v) {

  filedo(s, x.AlbedoColor, v);
  filedo(s, x.Roughness, v);
  filedo(s, x.foamDepthFalloff, v);
  filedo(s, x.foamRoughnessModifier, v);
  filedo(s, x.NormalDepthAttenuation, v);
  filedo(s, x._HeightModifier, v);
  filedo(s, x._WavePeakScatterStrength, v);
  filedo(s, x._ScatterShadowStrength, v);
  filedo(s, x._Fresnel, v);
}
void inn(std::ios *s, PixelLighting &x, bool v) {
  filedo(s, x.lightCount, v);
  for (auto &el : x.lights) {
    filedo(s, el.lightPos, v);
    filedo(s, el.lightColor, v);
    filedo(s, el.AmbientColor, v);
  }
}
void inn(std::ios *s, DeferredShading::DeferredShaderBuffers &x, bool v) {

  filedo(s, x._TipColor, v);
  filedo(s, x.EnvMapMult, v);
}

void inn(std::ios *s, RuntimeSettings &x, bool v) {
  filedo(s, x.timeRunning, v);
  filedo(s, x.showImgui, v);
  filedo(s, x.clearColor, v);
}
void inn(std::ios *s, Camera &x, bool v) {
  bool firstPerson = x.GetFirstPerson();
  auto eye = x.GetEye();
  auto at = x.GetAt();
  auto distance = x.GetDistance();
  filedo(s, firstPerson, v);
  filedo(s, eye, v);
  filedo(s, at, v);
  filedo(s, distance, v);
  x.SetFirstPerson(firstPerson);
  x.SetDistanceFromAt(distance);
  x.SetView(eye, at, x.GetWorldUp());
}

void PerformFileOperation(
    std::ios *stream, bool save, DebugValues &debugValues,
    SimulationData &simData,
    WaterGraphicRootDescription::WaterPixelShaderData &waterData,
    PixelLighting &sunData,
    DeferredShading::DeferredShaderBuffers &deferredData,
    RuntimeSettings &settings, Camera &cam, NeedToDo &beforeNextFrame) {

  inn(stream, debugValues, save);
  inn(stream, simData, save);
  inn(stream, waterData, save);
  inn(stream, sunData, save);
  inn(stream, deferredData, save);
  inn(stream, settings, save);
  inn(stream, cam, save);
  if (!save) {
    beforeNextFrame.patchHighestChanged = true;
    beforeNextFrame.patchMediumChanged = true;
    beforeNextFrame.patchLowestChanged = true;
  }
}

void ShowImguiLoaderConfig(
    DebugValues &debugValues, SimulationData &simData,
    WaterGraphicRootDescription::WaterPixelShaderData &waterData,
    PixelLighting &sunData,
    DeferredShading::DeferredShaderBuffers &deferredData,
    RuntimeSettings &settings, Camera &cam, NeedToDo &beforeNextFrame,
    bool exclusiveWindow) {

  static std::vector<std::pair<std::string, std::filesystem::path>> files =
      getFiles();
  static std::string Text = "";
  Text.reserve(128);
  static u16 selectedFile = 0;
  static bool canOverwrite = false;
  static bool canOverSave = false;
  static bool canDelete = false;

  bool pressedSave = false;
  bool pressedLoad = false;
  bool pressedDelete = false;

  bool cont = true;
  if (exclusiveWindow)
    cont = ImGui::Begin("Save data");
  if (cont) {
    if (!files.empty()) {

      if (ImGui::BeginCombo("File", files[selectedFile].first.c_str())) {
        for (uint i = 0; i < files.size(); i++) {
          bool isSelected = (selectedFile == i);
          if (ImGui::Selectable(files[i].first.c_str(), isSelected)) {
            selectedFile = i;
          }
          if (isSelected) {
            ImGui::SetItemDefaultFocus();
          }
        }
        ImGui::EndCombo();
      }
      if (!canDelete)
        ImGui::BeginDisabled();
      if (ImGui::Button("Delete")) {
        canDelete = false;
        pressedDelete = true;
      }
      if (!canDelete && !pressedDelete)
        ImGui::EndDisabled();
      ImGui::SameLine();
      ImGui::Checkbox("sure?##DeleteCheck", &canDelete);
    }

    ImGui::Checkbox("sure?##SaveCheck", &canOverSave);
    ImGui::SameLine();
    if (!canOverSave)
      ImGui::BeginDisabled();
    if (ImGui::Button("Save")) {
      pressedSave = true;
      canOverSave = false;
    }
    if (!canOverSave && !pressedSave)
      ImGui::EndDisabled();

    if (Text == "")
      ImGui::BeginDisabled();
    if (ImGui::Button("Create!")) {
      namespace fs = std::filesystem;
      fs::path dir = fs::path(GetLocalFolder()) / "SimConfig";

      if (!fs::exists(dir)) {
        fs::create_directories(dir); // Create directories if they don't exist
      }
      fs::path file = dir / Text;
      std::ofstream os(file);
      os.close();
      files = getFiles();
      auto it = std::find_if(files.begin(), files.end(), [](const auto &pair) {
        return pair.first == Text;
      });

      if (it != files.end()) {
        selectedFile = std::distance(files.begin(), it);
        pressedSave = true;
      }
    }
    if (Text == "")
      ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::InputText("New file: ##Createnewfile", Text.data(), 128)) {
      Text.resize(strlen(Text.c_str()));
    }

    ImGui::Checkbox("sure?##LoadCheck", &canOverwrite);
    ImGui::SameLine();
    if (!canOverwrite)
      ImGui::BeginDisabled();
    if (ImGui::Button("Load")) {
      pressedLoad = true;
      canOverwrite = false;
    }
    if (!canOverwrite && !pressedLoad)
      ImGui::EndDisabled();
  }
  if (exclusiveWindow)
    ImGui::End();

  if (pressedSave) {
    std::ofstream os(files[selectedFile].second);
    PerformFileOperation(&os, true, debugValues, simData, waterData, sunData,
                         deferredData, settings, cam, beforeNextFrame);
    os.close();
  }
  if (pressedLoad) {
    std::ifstream os(files[selectedFile].second);
    PerformFileOperation(&os, false, debugValues, simData, waterData, sunData,
                         deferredData, settings, cam, beforeNextFrame);
    os.close();
  }
  if (pressedDelete) {
    namespace fs = std::filesystem;
    fs::remove(files[selectedFile].second);
    files = getFiles();
  }
}
