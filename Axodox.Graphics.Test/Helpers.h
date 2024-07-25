#pragma once
#include "pch.h"

using namespace std;
using namespace DirectX;
using namespace DirectX::PackedVector;

using namespace Axodox::Graphics::D3D12;
using namespace Axodox::Infrastructure;
using namespace Axodox::Storage;

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
ImmutableTexture constexpr ImmutableTextureFromData(
    const ResourceAllocationContext &context, const Format &f, const u32 width,
    const u32 height, const u16 arraySize, std::span<const DataType> data) {
  auto text = TextureData(f, width, height, arraySize);
  std::span<u8> span = text.AsRawSpan();
  assert(span.size_bytes() == data.size_bytes());

  std::memcpy(span.data(), data.data(), data.size_bytes());
  return ImmutableTexture{context, text};
};

MeshDescription CreateQuadPatch() {
  float size = 1.f;
  size = size / 2;

  MeshDescription result;

  // result.Vertices = {VertexPosition{XMFLOAT3{-size, size, 0.f}},
  //                    VertexPosition{XMFLOAT3{-size, -size, 0.f}},
  //                    VertexPosition{XMFLOAT3{size, size, 0.f}},
  //                    VertexPosition{XMFLOAT3{size, -size, 0.f}}};
  result.Vertices = {VertexPosition{XMFLOAT3{-size, 0.f, size}},
                     VertexPosition{XMFLOAT3{size, 0, size}},
                     VertexPosition{XMFLOAT3{-size, 0.f, -size}},
                     VertexPosition{XMFLOAT3{size, 0.f, -size}}};

  result.Topology = static_cast<PrimitiveTopology>(
      D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);

  return result;
}
