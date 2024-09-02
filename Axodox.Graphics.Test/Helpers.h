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
TextureData constexpr CreateTextureData(const Format &f, const u32 width,
                                        const u32 height, const u16 arraySize,
                                        const std::vector<DataType> &data) {
  const u8 *dataPtr = reinterpret_cast<const u8 *>(data.data());

  std::size_t byteSize = data.size() * sizeof(DataType);

  const auto span = std::span<const u8>(dataPtr, byteSize);
  return TextureData(f, width, height, arraySize, span);
}

template <typename DataType>
TextureData constexpr CreateTextureData(const Format &f, const u32 width,
                                        const u32 height, const u16 arraySize,
                                        std::span<const u8> span) {
  return TextureData(f, width, height, arraySize, span);
}

inline MeshDescription CreateQuadPatch() {

  float size = 0.5f;
  MeshDescription result;

  result.Vertices = {VertexPosition{XMFLOAT3{-size, 0.f, size}},
                     VertexPosition{XMFLOAT3{size, 0, size}},
                     VertexPosition{XMFLOAT3{-size, 0.f, -size}},
                     VertexPosition{XMFLOAT3{size, 0.f, -size}}};

  result.Topology = static_cast<PrimitiveTopology>(
      D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);

  return result;
}

static ResourceStates GetResourceStateFromFlags(const TextureFlags &flags) {
  if (has_flag(flags, TextureFlags::RenderTarget)) {
    return ResourceStates::RenderTarget;
  } else if (has_flag(flags, TextureFlags::DepthStencil)) {
    return ResourceStates::DepthWrite;
  } else if (has_flag(flags, TextureFlags::ShaderResourceDepthStencil)) {
    return ResourceStates::DepthWrite;
  }

  else if (has_flag(flags, TextureFlags::UnorderedAccess)) {
    return ResourceStates::UnorderedAccess;
  } else {
    return ResourceStates::Common;
  }
}

class MutableTextureWithState
    : private Axodox::Graphics::D3D12::MutableTexture {

  ResourceStates _state;

public:
  MutableTextureWithState(const ResourceAllocationContext &context,
                          const TextureDefinition &definition)
      : MutableTexture(context, definition),
        _state(GetResourceStateFromFlags(definition.Flags)) {}

  // On the GPU this will happen when the queue reaches this command,
  // on the CPU this will happen when this function is executed.
  // Therefore if this resource is only used linearly this is fine,
  // but if multiple CPU threads or GPU queues use this function the state
  // will not be correct.
  void Transition(CommandAllocator &allocator, const ResourceStates &newState) {
    if (_state == newState)
      return;
    allocator.TransitionResource(
        this->operator Axodox::Graphics::D3D12::ResourceArgument(), _state,
        newState);
    _state = newState;
  }

  // On the GPU this will happen when the queue reaches this command,
  // on the CPU this will happen when this function is executed.
  // Therefore if this resource is only used linearly this is fine,
  // but if multiple CPU threads or GPU queues use this function the state
  // will not be correct.
  ShaderResourceView *ShaderResource(CommandAllocator &allocator) {
    Transition(allocator, ResourceStates::AllShaderResource);
    return MutableTexture::ShaderResource();
  };

  // On the GPU this will happen when the queue reaches this command,
  // on the CPU this will happen when this function is executed.
  // Therefore if this resource is only used linearly this is fine,
  // but if multiple CPU threads or GPU queues use this function the state
  // will not be correct.

  UnorderedAccessView *UnorderedAccess(CommandAllocator &allocator) {

    Transition(allocator, ResourceStates::UnorderedAccess);
    return MutableTexture::UnorderedAccess();
  };

  MutableTexture &getInnerUnsafe() { return *this; }
};

inline float frac(float x) { return x - std::floor(x); }

struct NeedToDo {
  std::optional<RasterizerFlags> changeFlag;
  bool PatchHighestChanged = false;
  bool PatchMediumChanged = false;
  bool PatchLowestChanged = false;
};

/// <summary>
/// For deferred shading the culling should be set to CullCounterClockwise
/// therefore shown mesh should be facing backwards
/// </summary>
/// <param name="size"></param>
/// <param name="subdivisions"></param>
/// <returns></returns>
inline MeshDescription CreateBackwardsPlane(float size,
                                            DirectX::XMUINT2 subdivisions) {
  if (subdivisions.x < 2 || subdivisions.y < 2)
    throw logic_error("Plane size must be at least 2!");
  if (subdivisions.x * subdivisions.y >
      (uint64_t)numeric_limits<uint32_t>::max() + 1)
    throw logic_error("Run out of indices!");

  MeshDescription result;

  // Vertices
  float xstep = size / (subdivisions.x - 1),
        xtexstep = 1.f / (subdivisions.x - 1), xstart = -size / 2.f;
  float ystep = size / (subdivisions.y - 1),
        ytexstep = 1.f / (subdivisions.y - 1), ystart = -size / 2.f;
  uint32_t vertexCount = subdivisions.x * subdivisions.y;

  VertexPositionNormalTexture *pVertex;
  result.Vertices = BufferData(vertexCount, pVertex);

  for (uint32_t j = 0; j < subdivisions.y; j++) {
    for (uint32_t i = 0; i < subdivisions.x; i++) {
      *pVertex++ = {XMFLOAT3{xstart + i * xstep, ystart + j * ystep, 0.f},
                    XMBYTEN4{0.f, 0.f, 1.f, 1.f},
                    XMUSHORTN2{i * xtexstep, 1 - j * ytexstep}};
    }
  }

  // Indices
  uint32_t triangleWidth = subdivisions.x - 1,
           triangleHeight = subdivisions.y - 1;
  uint32_t indexCount = triangleWidth * triangleHeight * 6;

  uint32_t *pIndex;
  result.Indices = BufferData(indexCount, pIndex);

  for (uint32_t j = 0; j < triangleHeight; j++) {
    for (uint32_t i = 0; i < triangleWidth; i++) {
      *pIndex++ = j * subdivisions.x + i;
      *pIndex++ = (j + 1) * subdivisions.x + i;
      *pIndex++ = j * subdivisions.x + i + 1;
      *pIndex++ = j * subdivisions.x + i + 1;
      *pIndex++ = (j + 1) * subdivisions.x + i;
      *pIndex++ = (j + 1) * subdivisions.x + i + 1;
    }
  }

  // Topology
  result.Topology = PrimitiveTopology::TriangleList;

  return result;
}

template <typename T, const size_t N>
std::initializer_list<typename std::array<T, N>::value_type>
to_initializer_list(const std::array<T, N> &arr) {

  return std::initializer_list<typename std::array<T, N>::value_type>(
      arr.data(), arr.data() + arr.size());
}
