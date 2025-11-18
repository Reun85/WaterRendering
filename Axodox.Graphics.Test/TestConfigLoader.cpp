#include "pch.h"
#include <filesystem>
#include <iostream>
#include <fstream>

#include "Simulation.h"
#include "GraphicsPipeline.h"
#include "DebugValues.h"

#include "TestConfigLoader.h"

namespace Reun {
// Helper streams
// -----------------------------------------------------------------------------
struct DataOutStream {
  explicit DataOutStream(std::ostream &os) : os(os) {}
  std::ostream &operator*() { return os; }
  std::ostream *operator->() { return &os; }

private:
  std::ostream &os;
};

struct DataInStream {
  explicit DataInStream(std::istream &is) : is(is) {}

  std::istream &operator*() { return is; }
  std::istream *operator->() { return &is; }

private:
  std::istream &is;
};

template <typename T>
concept MyStream = Either<T, DataOutStream, DataInStream>;

// -----------------------------------------------------------------------------

// If it has a default stream operator, just use that.
// -----------------------------------------------------------------------------
template <typename T>
concept OStreamWritable = requires(std::ostream &os, T value) {
  { os << value } -> std::same_as<std::ostream &>;
};

template <OStreamWritable T>
DataOutStream &operator<<(DataOutStream &os, const T &x) {
  *os << x;
  return os;
}

template <typename T>
concept IStreamReadable = requires(std::istream &is, T &value) {
  { is >> value } -> std::same_as<std::istream &>;
};
template <IStreamReadable T> DataInStream &operator>>(DataInStream &is, T &x) {
  *is >> x;
  return is;
}

template <typename T>
concept MyStreamWriteable = requires(DataOutStream &os, const T &value) {
  { os << value } -> std::same_as<DataOutStream &>;
};
template <typename T>
concept MyStreamReadable = requires(DataInStream &is, T &value) {
  { is >> value } -> std::same_as<DataInStream &>;
};

// -----------------------------------------------------------------------------

// Enum serialization
// -----------------------------------------------------------------------------
template <typename T>
concept EnumType = std::is_enum_v<T>;

template <EnumType Enum>
DataOutStream &operator<<(DataOutStream &os, const Enum &value) {
  using UnderlyingType = std::underlying_type_t<Enum>;
  *os << static_cast<UnderlyingType>(value);
  return os;
}

template <EnumType Enum>
DataInStream &operator>>(DataInStream &is, Enum &value) {
  using UnderlyingType = std::underlying_type_t<Enum>;
  UnderlyingType temp;
  *is >> temp;
  value = static_cast<Enum>(temp); // Cast back to Enum type
  return is;
}

// Optional enum type
template <EnumType Enum> class AsShiftedOptionalEnum {
public:
  using UnderlyingType = std::underlying_type_t<Enum>;
  // If we print/read as a char, we will get nonsensical values!
    using ReadType = std::conditional_t<
        std::is_same_v<UnderlyingType, unsigned char> ||
        std::is_same_v<UnderlyingType, char>,
        int,
        UnderlyingType
    >;
  explicit AsShiftedOptionalEnum(std::optional<Enum> &val) : val_(val) {}

  friend DataOutStream &operator<<(DataOutStream &os,
                                   const AsShiftedOptionalEnum<Enum> &inp) {

    if (inp.val_.has_value()) {
      const auto val = inp.val_.value();
      const auto underlying = static_cast<ReadType>(val);
      const auto printed = underlying + 1;
      *os << printed;
    } else {
      *os << 0;
    }
    return os;
  }

  friend DataInStream &operator>>(DataInStream &is,
                                  AsShiftedOptionalEnum<Enum> &inp) {

    auto &value = inp.val_;
    // 0 is definitely inside the UnderlyingType range
    ReadType temp;
    is >> temp;
    if (temp == 0) {
      value = std::nullopt;
    }

    else {
      temp -= 1;
      value = Enum(temp); // Cast back to Enum type
    }
    return is;
  }

private:
  std::optional<Enum> &val_;
};

// -----------------------------------------------------------------------------

// Collection serialization
// -----------------------------------------------------------------------------
template <typename T>
DataInStream &operator>>(DataInStream &is, std::vector<T> &v) {
  for (auto &el : v) {
    is >> el;
  }
  return is;
}

template <typename T>
DataOutStream &operator<<(DataOutStream &os, const std::vector<T> &v) {
  for (auto &el : v) {
    os << el << " ";
  }
  return os;
}

template <typename T, const u32 N>
DataInStream &operator>>(DataInStream &is, std::array<T, N> &v) {
  for (auto &el : v) {
    is >> el;
  }
  return is;
}

template <typename T, const u32 N>
DataOutStream &operator<<(DataOutStream &os, const std::array<T, N> &v) {
  for (auto &el : v) {
    os << el << " ";
  }
  return os;
}

// -----------------------------------------------------------------------------

// DirectX / WinRT types
// -----------------------------------------------------------------------------
DataInStream &operator>>(DataInStream &is, float2 &x) {
  is >> x.x >> x.y;
  return is;
}

DataOutStream &operator<<(DataOutStream &os, const float2 &x) {
  os << x.x << " " << x.y;
  return os;
}
DataInStream &operator>>(DataInStream &is, float3 &x) {
  is >> x.x >> x.y >> x.z;
  return is;
}

DataOutStream &operator<<(DataOutStream &os, const float3 &x) {
  os << x.x << " " << x.y << " " << x.z;
  return os;
}

DataInStream &operator>>(DataInStream &is, float4 &x) {
  is >> x.x >> x.y >> x.z >> x.w;
  return is;
}

DataOutStream &operator<<(DataOutStream &os, const float4 &x) {
  os << x.x << " " << x.y << " " << x.z << " " << x.w;
  return os;
}

DataInStream &operator>>(DataInStream &is, XMFLOAT3 &x) {
  is >> x.x >> x.y >> x.z;
  return is;
}

DataOutStream &operator<<(DataOutStream &os, const XMFLOAT3 &x) {
  os << x.x << " " << x.y << " " << x.z;
  return os;
}
DataInStream &operator>>(DataInStream &is, XMFLOAT4 &x) {
  is >> x.x >> x.y >> x.z >> x.w;
  return is;
}

DataOutStream &operator<<(DataOutStream &os, const XMFLOAT4 &x) {
  os << x.x << " " << x.y << " " << x.z << " " << x.w;
  return os;
}

DataInStream &operator>>(DataInStream &is, XMUINT4 &x) {
  is >> x.x >> x.y >> x.z >> x.w;
  return is;
}

DataOutStream &operator<<(DataOutStream &os, const XMUINT4 &x) {
  os << x.x << " " << x.y << " " << x.z << " " << x.w;
  return os;
}

DataInStream &operator>>(DataInStream &is, XMVECTOR &y) {
  XMFLOAT4 x;
  XMStoreFloat4(&x, y);
  is >> x.x >> x.y >> x.z >> x.w;
  y = XMLoadFloat4(&x);
  return is;
}

DataOutStream &operator<<(DataOutStream &os, const XMVECTOR &y) {
  XMFLOAT4 x;
  XMStoreFloat4(&x, y);
  os << x.x << " " << x.y << " " << x.z << " " << x.w;
  return os;
}

// -----------------------------------------------------------------------------

template <MyStreamWriteable T> void streamdo(DataOutStream &os, const T &val) {
  os << val << "\n";
}
template <MyStreamReadable T> void streamdo(DataInStream &is, T &val) {
  is >> val;
}

template <MyStreamWriteable T> void streamdo(DataOutStream &os, const T &&val) {
  os << val << "\n";
}
template <MyStreamReadable T> void streamdo(DataInStream &is, T &&val) {
  is >> val;
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
  } catch (std::exception &) {
  }
  return files;
}
template <MyStream OS> void HandleDebugValues(OS &s, Reun::DebugValues &x) {
  streamdo(s, x.conecreater);
  streamdo(s, x.pixelMult);
  streamdo(s, x.swizzleorder);
  streamdo(s, x.blendDistances);
  streamdo(s, x.foamColor);
  streamdo(s, x.DebugBits);
  streamdo(s, x.enableSSR);
  streamdo(s, x.lockQuadTree);
  streamdo(s, x.maxConeStep);
  streamdo(s, x.prismHeight);
  streamdo(s, x.coneStepRelax);
  streamdo(s, x.drawMethod);
  streamdo(s, AsShiftedOptionalEnum(x.debugTextureMode));
  streamdo(s, x.rasterizerFlags);
}
template <MyStream OS>
void HandlesPatchData(OS &s, SimulationData::PatchData &x) {

  streamdo(s, x.displacementLambda);
  streamdo(s, x.patchSize);
  streamdo(s, x.patchExtent);
  streamdo(s, x.foamExponentialDecay);
  streamdo(s, x.amplitude);
  streamdo(s, x.windForce);
  streamdo(s, x.foamMinValue);
  streamdo(s, x.foamBias);
  streamdo(s, x.foamMult);
  // streamdo(s, x.N);
  // streamdo(s, x.M);
  streamdo(s, x.windDirection);
  streamdo(s, x.gravity);
  streamdo(s, x.depth);
};
template <MyStream OS>
void HandleSimulationData(
    OS &s, Reun::SimulationData &x,
    std::optional<Reun::NeedToDo *> beforeNextFrame = std::nullopt) {
  SimulationData::PatchData tmp = x.highest;
  HandlesPatchData(s, x.highest);
  if (beforeNextFrame.has_value()) {
    NeedToDo &b = **beforeNextFrame;
    b.patchHighestChanged = !x.highest.compatibleSim(tmp);
  }
  tmp = x.medium;
  HandlesPatchData(s, x.medium);
  if (beforeNextFrame.has_value()) {
    NeedToDo &b = **beforeNextFrame;
    b.patchMediumChanged = !x.medium.compatibleSim(tmp);
  }
  tmp = x.lowest;
  HandlesPatchData(s, x.lowest);
  if (beforeNextFrame.has_value()) {
    NeedToDo &b = **beforeNextFrame;
    b.patchLowestChanged = !x.lowest.compatibleSim(tmp);
  }
  // streamdo(s, x.N, v);
  // streamdo(s, x.M, v);
  streamdo(s, x.windDirection);
  streamdo(s, x.gravity);
  streamdo(s, x.depth);
  streamdo(s, x.quadTreeDistanceThreshold);
  streamdo(s, x.maxDepth);
}
template <MyStream OS>
void HandleWaterPixelShaderData(
    OS &s,
    Reun::Graphics::WaterGraphicRootDescription::WaterPixelShaderData &x) {

  streamdo(s, x.AlbedoColor);
  streamdo(s, x.Roughness);
  streamdo(s, x.foamDepthFalloff);
  streamdo(s, x.foamRoughnessModifier);
  streamdo(s, x.NormalDepthAttenuation);
  streamdo(s, x._HeightModifier);
  streamdo(s, x._WavePeakScatterStrength);
  streamdo(s, x._ScatterShadowStrength);
  streamdo(s, x._Fresnel);
}
template <MyStream OS>
void HandlePixelLighting(OS &s, Reun::Graphics::PixelLighting &x) {
  streamdo(s, x.lightCount);
  for (auto &el : x.lights) {
    streamdo(s, el.lightPos);
    streamdo(s, el.lightColor);
    streamdo(s, el.AmbientColor);
  }
}
template <MyStream OS>
void HandleDeferredShader(
    OS &s, Reun::Graphics::DeferredShading::DeferredShaderBuffers &x) {

  streamdo(s, x._TipColor);
  streamdo(s, x.EnvMapMult);
}
template <MyStream OS>
void HandleRuntimeSettings(OS &s, Reun::RuntimeSettings &x) {
  streamdo(s, x.timeRunning);
  streamdo(s, x.showImgui);
  streamdo(s, x.clearColor);
}
template <MyStream OS> void HandleCamera(OS &s, Reun::Camera &x) {
  bool firstPerson = x.GetFirstPerson();
  auto eye = x.GetEye();
  auto at = x.GetAt();
  auto distance = x.GetDistance();
  streamdo(s, firstPerson);
  streamdo(s, eye);
  streamdo(s, at);
  streamdo(s, distance);
  x.SetFirstPerson(firstPerson);
  x.SetDistanceFromAt(distance);
  x.SetView(eye, at, x.GetWorldUp());
}

template <MyStream OS>
void PerformFileOperation(
    OS &stream, DebugValues &debugValues, SimulationData &simData,
    Reun::Graphics::WaterGraphicRootDescription::WaterPixelShaderData
        &waterData,
    Reun::Graphics::PixelLighting &sunData,
    Reun::Graphics::DeferredShading::DeferredShaderBuffers &deferredData,
    RuntimeSettings &settings, Camera &cam, NeedToDo &beforeNextFrame) {

  HandleDebugValues(stream, debugValues);
  HandleSimulationData(stream, simData, &beforeNextFrame);
  HandleWaterPixelShaderData(stream, waterData);
  HandlePixelLighting(stream, sunData);
  HandleDeferredShader(stream, deferredData);
  HandleRuntimeSettings(stream, settings);
  HandleCamera(stream, cam);
}

void Reun::ShowImguiLoaderConfig(
    Reun::DebugValues &debugValues, Reun::SimulationData &simData,
    Graphics::WaterGraphicRootDescription::WaterPixelShaderData &waterData,
    Graphics::PixelLighting &sunData,
    Graphics::DeferredShading::DeferredShaderBuffers &deferredData,
    Reun::RuntimeSettings &settings, Reun::Camera &cam,
    Reun::NeedToDo &beforeNextFrame) {

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

  if (ImGui::Button("Reload file list")) {
    selectedFile = 0;
    canOverwrite = false;
    canOverSave = false;
    canDelete = false;
    files = getFiles();
  }
  if (!files.empty()) {

    ImGui::Text("Selected file for action:");
    if (ImGui::BeginCombo("File", files[selectedFile].first.c_str())) {
      for (u16 i = 0; i < files.size(); i++) {
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

    ImGui::SeparatorText("File actions:");
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

    ImGui::Separator();
  }

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
    auto it = std::ranges::find_if(
        files, [](const auto &pair) { return pair.first == Text; });

    if (it != files.end()) {
      selectedFile = static_cast<u16>(std::distance(files.begin(), it));
      pressedSave = true;
    }
  }
  if (Text == "")
    ImGui::EndDisabled();
  ImGui::SameLine();
  if (ImGui::InputText("New file: ##Createnewfile", Text.data(), 128)) {
    Text.resize(strlen(Text.c_str()));
  }
  // Do the chosen operations
  if (pressedSave) {
    std::ofstream os(files[selectedFile].second);
    DataOutStream s(os);
    PerformFileOperation(s, debugValues, simData, waterData, sunData,
                         deferredData, settings, cam, beforeNextFrame);
  }
  if (pressedLoad) {
    std::ifstream os(files[selectedFile].second);
    DataInStream s(os);
    PerformFileOperation(s, debugValues, simData, waterData, sunData,
                         deferredData, settings, cam, beforeNextFrame);
    settings.timeRunning = false;
  }
  if (pressedDelete) {
    std::filesystem::remove(files[selectedFile].second);
    files = getFiles();
  }
}
} // namespace Reun
