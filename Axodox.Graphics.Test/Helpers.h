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
  } else if (has_flag(flags, TextureFlags::UnorderedAccess)) {
    return ResourceStates::UnorderedAccess;
  } else {
    return ResourceStates::Common;
  }
}

// A class meant to simplify keeping track of a GPU resource's state.
// If multiple threads (cpu or gpu command queues) use this in parallel, this
// abstraction will cause more issues than it solves.
class MutableTextureWithState
    : private Axodox::Graphics::D3D12::MutableTexture {
  ResourceStates _state;

public:
  MutableTextureWithState(const ResourceAllocationContext &context,
                          const TextureDefinition &definition)
      : MutableTexture(context, definition),
        _state(GetResourceStateFromFlags(definition.Flags)) {}

  // On the GPU the transition happen when the queue reaches this command,
  // on the CPU the "transition" will happen when this function is executed.
  // Therefore if this resource is only used linearly this is fine,
  // but if multiple CPU threads or GPU queues use this resource this
  // abstraction will cause more issues than it solves.
  void Transition(CommandAllocator &allocator, const ResourceStates &newState) {
    if (_state == newState)
      return;
    allocator.TransitionResource(
        this->operator Axodox::Graphics::D3D12::ResourceArgument(), _state,
        newState);
    _state = newState;
  }

  // On the GPU the transition happen when the queue reaches this command,
  // on the CPU the "transition" will happen when this function is executed.
  // Therefore if this resource is only used linearly this is fine,
  // but if multiple CPU threads or GPU queues use this resource this
  // abstraction will cause more issues than it solves.
  ShaderResourceView *ShaderResource(CommandAllocator &allocator) {
    Transition(allocator, ResourceStates::AllShaderResource);
    return MutableTexture::ShaderResource();
  };

  // On the GPU the transition happen when the queue reaches this command,
  // on the CPU the "transition" will happen when this function is executed.
  // Therefore if this resource is only used linearly this is fine,
  // but if multiple CPU threads or GPU queues use this resource this
  // abstraction will cause more issues than it solves.
  UnorderedAccessView *UnorderedAccess(CommandAllocator &allocator) {
    Transition(allocator, ResourceStates::UnorderedAccess);
    return MutableTexture::UnorderedAccess();
  };

  MutableTexture &getInnerUnsafe() { return *this; }
};

inline float frac(float x) { return x - std::floor(x); }

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
                    XMUSHORTN2{i * xtexstep, j * ytexstep}};
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

inline MeshDescription
CreateCubeWithoutBottom(float size, XMFLOAT3 offset = XMFLOAT3{0, 0, 0}) {
  MeshDescription result;
  size = size / 2;

  // Vertices
  VertexPosition *pVertex;
  result.Vertices = BufferData(8, pVertex);

  *pVertex++ = VertexPosition{XMFLOAT3{size, size, -size}};
  *pVertex++ = VertexPosition{XMFLOAT3{size, size, size}};
  *pVertex++ = VertexPosition{XMFLOAT3{-size, size, -size}};
  *pVertex++ = VertexPosition{XMFLOAT3{-size, size, size}};

  *pVertex++ = VertexPosition{XMFLOAT3{size, -size, -size}};
  *pVertex++ = VertexPosition{XMFLOAT3{size, -size, size}};
  *pVertex++ = VertexPosition{XMFLOAT3{-size, -size, -size}};
  *pVertex++ = VertexPosition{XMFLOAT3{-size, -size, size}};
  for (int i = -(i32)result.Vertices.ItemCount(); i < 0; i++) {
    pVertex[i].Position.x += offset.x;
    pVertex[i].Position.y += offset.y;
    pVertex[i].Position.z += offset.z;
  }

  // Indices
  uint32_t *pIndex;
  result.Indices = BufferData(5 * 2 * 3, pIndex);

  // Top Face (1, 2, 3, 4)
  *pIndex++ = 0;
  *pIndex++ = 2;
  *pIndex++ = 1; // Triangle 1
  *pIndex++ = 1;
  *pIndex++ = 2;
  *pIndex++ = 3; // Triangle 2

  // Front Face (1, 3, 5, 7)
  *pIndex++ = 0;
  *pIndex++ = 4;
  *pIndex++ = 2; // Triangle 1
  *pIndex++ = 2;
  *pIndex++ = 4;
  *pIndex++ = 6; // Triangle 2

  // Back Face (2, 4, 6, 8)
  *pIndex++ = 1;
  *pIndex++ = 3;
  *pIndex++ = 5; // Triangle 1
  *pIndex++ = 5;
  *pIndex++ = 3;
  *pIndex++ = 7; // Triangle 2

  // Left Face (3, 4, 7, 8)
  *pIndex++ = 2;
  *pIndex++ = 6;
  *pIndex++ = 3; // Triangle 1
  *pIndex++ = 3;
  *pIndex++ = 6;
  *pIndex++ = 7; // Triangle 2

  // Right Face (1, 2, 5, 6)
  *pIndex++ = 0;
  *pIndex++ = 1;
  *pIndex++ = 4; // Triangle 1
  *pIndex++ = 4;
  *pIndex++ = 1;
  *pIndex++ = 5; // Triangle 2

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
  void DrawImGui(bool exclusiveWindow = false) const {
    bool cont = true;
    if (exclusiveWindow)
      cont = ImGui::Begin("Results");
    if (cont) {
      ImGui::Text("QuadTree Nodes = %d", qtNodes);

      ImGui::Text("QuadTree buildtime %.3f ms/frame",
                  GetDurationInFloatWithPrecision<std::chrono::milliseconds,
                                                  std::chrono::nanoseconds>(
                      QuadTreeBuildTime));
      ImGui::Text("Navigating QuadTree %.3f ms/frame",
                  GetDurationInFloatWithPrecision<std::chrono::milliseconds,
                                                  std::chrono::nanoseconds>(
                      NavigatingTheQuadTree));
      ImGui::Text("Drawn Nodes: %d", drawnNodes);
      ImGui::Text(
          "CPU time %.3f ms/frame",
          GetDurationInFloatWithPrecision<std::chrono::milliseconds,
                                          std::chrono::nanoseconds>((CPUTime)));
    }
    if (exclusiveWindow)
      ImGui::End();
  }
};

inline float3 XMVECTORToFloat3(const XMVECTOR &x) {
  float3 result;
  XMStoreFloat3(&result, x);
  return result;
}

inline float4x4 XMMatrixToFloat4x4(const XMMATRIX &x) {
  float4x4 result;
  XMStoreFloat4x4(&result, x);
  return result;
}

inline std::string GetLocalFolder() {
  auto localFolder =
      winrt::Windows::Storage::ApplicationData::Current().LocalFolder().Path();
  std::wstring localFolderWstr = localFolder.c_str();
  auto path = std::string(localFolderWstr.begin(), localFolderWstr.end());
  return path;
}

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
