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
  } else if (has_flag(flags, TextureFlags::UnorderedAccess)) {
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
