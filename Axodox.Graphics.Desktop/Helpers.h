#pragma once

#include "pch.h"
#include "MutableTextureWithState.hpp"
namespace Reun {

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
  static NeedToDo WithFirstLoopUpdateSettings();
};

template <typename T, const size_t N>
std::initializer_list<typename std::array<T, N>::value_type>
to_initializer_list(const std::array<T, N> &arr) {
  return std::initializer_list<typename std::array<T, N>::value_type>(
      arr.data(), arr.data() + arr.size());
}

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

struct CameraConstants {
  XMFLOAT4X4 vMatrix;
  XMFLOAT4X4 pMatrix;
  XMFLOAT4X4 vpMatrix;
  XMFLOAT4X4 INVvMatrix;
  XMFLOAT4X4 INVpMatrix;
  XMFLOAT4X4 INVvpMatrix;
  XMFLOAT3 cameraPos;
};

struct RuntimeResults {
  u32 qtNodes = 0;
  u32 drawnNodes = 0;
  std::chrono::nanoseconds QuadTreeBuildTime{0};

  std::chrono::nanoseconds NavigatingTheQuadTree{0};
  std::chrono::nanoseconds CPUTime{0};
  void DrawImGui(bool exclusiveWindow = false) const;
};

float3 XMVECTORToFloat3(const DirectX::XMVECTOR &x);
std::string Utf16ToUtf8(const std::wstring_view &wstr);
// just returns the string copied.
std::string Utf16ToUtf8(const std::string_view &str);
std::wstring Utf8ToUtf16(const std::string_view &str);
std::wstring Utf8ToUtf16(const std::wstring_view &str);
std::filesystem::path GetLocalFolder();
std::filesystem::path GetCacheFolder();

void set_flag(u32 &flag, u32 flagIndex, bool flagValue = true);
}; // namespace Reun
