#pragma once

#include "pch.h"
#include "MutableTextureWithState.hpp"

template <typename T, typename Left, typename Right>
concept Either = std::same_as<T, Left> || std::same_as<T, Right>;
using namespace DirectX::PackedVector;

template <typename T>
concept IsRatio = std::is_same_v<T, std::ratio<T::num, T::den>>;
template <typename T>
concept HasRatioPeriod =
    requires { typename T::period; } && IsRatio<typename T::period>;

template <typename TimeRep, typename PrecisionRep, typename T, typename Q>
  requires IsRatio<TimeRep> && IsRatio<PrecisionRep> && IsRatio<Q>
constexpr float
GetDurationInFloatWithPrecision(const std::chrono::duration<T, Q> &inp) {
  using Result = std::ratio_divide<PrecisionRep, TimeRep>;
  using Precision = std::chrono::duration<T, PrecisionRep>;
  const T count = std::chrono::duration_cast<Precision>(inp).count();
  return static_cast<float>(count) * static_cast<float>(Result::num) /
         static_cast<float>(Result::den);
}

template <typename TimeRepTimeFrame, typename PrecisionTimeFrame, typename T,
          typename Q>
  requires HasRatioPeriod<TimeRepTimeFrame> &&
           HasRatioPeriod<PrecisionTimeFrame> && IsRatio<Q>
constexpr float
GetDurationInFloatWithPrecision(const std::chrono::duration<T, Q> &inp) {
  return GetDurationInFloatWithPrecision<typename TimeRepTimeFrame::period,
                                         typename PrecisionTimeFrame::period, T,
                                         Q>(inp);
}

template <typename DataType>
TextureData constexpr CreateTextureData(const Format &f, const u32 width,
                                        const u32 height, const u16 arraySize,
                                        const std::vector<DataType> &data) {
  const auto *dataPtr = reinterpret_cast<const std::byte *>(data.data());

  usize byteSize = data.size() * sizeof(DataType);

  auto span = std::span<const u8>((u8 *)dataPtr, byteSize);
  return TextureData(f, width, height, arraySize, span);
}

float frac(float x);

struct NeedToDo {
  std::optional<RasterizerFlags> changeFlag;
  bool patchHighestChanged = false;
  bool patchMediumChanged = false;
  bool patchLowestChanged = false;
};

/// <summary>
/// For deferred shading the culling should be set to CullCounterClockwise
/// therefore shown mesh should be facing backwards
/// </summary>
/// <param name="size"></param>
/// <param name="subdivisions"></param>
/// <returns></returns>
MeshDescription CreateBackwardsPlane(float size, DirectX::XMUINT2 subdivisions);

MeshDescription CreateCubeWithoutBottom(float size,
                                        XMFLOAT3 offset = XMFLOAT3{0, 0, 0});

MeshDescription CreateBoxInVSMesh();

template <typename T, const size_t N>
std::initializer_list<typename std::array<T, N>::value_type>
to_initializer_list(const std::array<T, N> &arr) {
  return std::initializer_list<typename std::array<T, N>::value_type>(
      arr.data(), arr.data() + arr.size());
}

struct CameraConstants {
  XMFLOAT4X4 vMatrix;
  XMFLOAT4X4 pMatrix;
  XMFLOAT4X4 vpMatrix;
  XMFLOAT4X4 INVvMatrix;
  XMFLOAT4X4 INVpMatrix;
  XMFLOAT4X4 INVvpMatrix;
  XMFLOAT3 cameraPos;
};
struct LightData {
  XMFLOAT4 lightPos;   // .a 0 for directional, 1 for positional
  XMFLOAT4 lightColor; // .a is lightIntensity
  XMFLOAT4 AmbientColor;
};

struct PixelLighting {
  std::array<LightData, ShaderConstantCompat::maxLightCount> lights;
  int lightCount;

  static constexpr PixelLighting SunData() {
    PixelLighting data = {};
    data.lightCount = 1;
    data.lights[0].lightPos = XMFLOAT4(1.f, 0.109f, 0.964f, 0.f);
    data.lights[0].lightColor =
        XMFLOAT4(231.f / 255.f, 207.f / 255.f, 137.f / 255.f, 1.f);

    data.lights[0].AmbientColor =
        XMFLOAT4(15.f / 255.f, 14.f / 255.f, 5.f / 255.f, .185f);

    return data;
  }
  // old
private:
  static constexpr PixelLighting old() {
    PixelLighting data = {};
    data.lightCount = 1;
    data.lights[0].lightPos = XMFLOAT4(1, 0.109f, 0.964f, 0);
    data.lights[0].lightColor =
        XMFLOAT4(243.f / 255.f, 206.f / 255.f, 97.f / 255.f, 0.446f);

    data.lights[0].AmbientColor =
        XMFLOAT4(15.f / 255.f, 14.f / 255.f, 5.f / 255.f, .639f);

    return data;
  }
};

struct RuntimeResults {
  u32 qtNodes = 0;
  u32 drawnNodes = 0;
  std::chrono::nanoseconds QuadTreeBuildTime{0};

  std::chrono::nanoseconds NavigatingTheQuadTree{0};
  std::chrono::nanoseconds CPUTime{0};
  void DrawImGui(bool exclusiveWindow = false) const;
};

inline float3 XMVECTORToFloat3(const DirectX::XMVECTOR &x) {
  float3 result;
  DirectX::XMStoreFloat3(&result, x);
  return result;
}

float4x4 XMMatrixToFloat4x4(const DirectX::XMMATRIX &x);
std::string Utf16ToUtf8(const std::wstring_view &wstr);
std::wstring Utf8ToUtf16(const std::string_view &str);
std::string GetLocalFolder();

struct ShaderBuffers {
  // Allocates necessary buffers if they are not yet allocated. May use the
  // finalTarget size to determine the sizes of the buffers
  virtual void MakeCompatible(const RenderTargetView &finalTarget,
                              ResourceAllocationContext &allocationContext) = 0;

  // Get ready for next frame
  virtual void Clear(CommandAllocator &allocator) = 0;

  virtual ~ShaderBuffers() = default;
};

struct ShaderJob {
  virtual void Pre(CommandAllocator &allocator) const = 0;
  virtual ~ShaderJob() = default;
};

template <typename Lambda>
std::vector<std::invoke_result_t<Lambda>> inline NewVectorByFunction(
    const usize n, Lambda &&factory) {
  using T = std::invoke_result_t<Lambda>;
  std::vector<T> result;
  result.reserve(n);
  for (usize i = 0; i < n; i++) {
    result.push_back(std::forward<T>(factory()));
  }
  return result;
}

void set_flag(u32 &flag, u32 flagIndex, bool flagValue = true);

MeshDescription CreateQuadPatch();
