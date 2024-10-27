#pragma once
#include "pch.h"
#include "Defaults.h"
#include "Helpers.h"

using namespace DirectX;
using namespace DirectX::PackedVector;

using namespace Axodox::Infrastructure;
using namespace Axodox::Storage;
using namespace DirectX::PackedVector;
class Camera;

struct ShadowVolume : ShaderJob {
  struct ShaderMask : RootSignatureMask {
    explicit ShaderMask(const RootSignatureContext &context)
        : RootSignatureMask(context) {
      Flags = RootSignatureFlags::AllowInputAssemblerInputLayout;
    }
    constexpr static D3D12_DEPTH_STENCIL_DESC GetDepthStencilDesc();
  };

  struct Textures : ShaderBuffers {
    MutableTextureWithViews StencilBuffer;

    explicit Textures(const ResourceAllocationContext &context);

    void MakeCompatible(const RenderTargetView &finalTarget,
                        ResourceAllocationContext &allocationContext) override {
      // No need to allocate
    }
    void Clear(CommandAllocator &allocator) override;
    void TranslateToTarget(CommandAllocator &allocator);
    void TranslateToView(
        CommandAllocator &allocator,
        const ResourceStates &newState = ResourceStates::AllShaderResource);
    ~Textures() override = default;
  };

  struct Inp {
    Textures buffers;
    GpuVirtualAddress camera;
    ImmutableMesh &mesh;
  };

  RootSignature<ShaderMask> Signature;
  PipelineState pipeline;

  ShadowVolume(PipelineStateProvider &pipelineProvider, GraphicsDevice &device,
               VertexShader *vs, GeometryShader *gs, PixelShader *ps);

  static ShadowVolume
  WithDefaultShaders(PipelineStateProvider &pipelineProvider,
                     GraphicsDevice &device);
  void Pre(CommandAllocator &allocator) const override {
    pipeline.Apply(allocator);
  }
  void Run(CommandAllocator &allocator, DynamicBufferManager &buffermanager,
           const Inp &inp) const;
  ~ShadowVolume() override = default;
};

struct SilhouetteDetector : ShaderJob {
  struct ShaderMask : public RootSignatureMask {
    // In
    RootDescriptorTable<1> Vertex;
    RootDescriptorTable<1> Index;

    // Out
    RootDescriptorTable<1> Edges;
    RootDescriptorTable<1> EdgeCount;

    RootDescriptor<RootDescriptorType::ConstantBuffer> lights;
    RootDescriptor<RootDescriptorType::ConstantBuffer> constants;

    explicit ShaderMask(const RootSignatureContext &context)
        : RootSignatureMask(context),

          Vertex(this, {DescriptorRangeType::ShaderResource, {0}}),
          Index(this, {DescriptorRangeType::ShaderResource, {1}}),
          Edges(this, {DescriptorRangeType::UnorderedAccess, {0}}),
          EdgeCount(this, {DescriptorRangeType::UnorderedAccess, {1}}),
          lights(this, {1}), constants(this, {0})

    {
      Flags = RootSignatureFlags::None;
    }
  };

  struct Buffers {
    struct EdgeBufferType {
      std::pair<u32, u32> vertices;
      std::pair<i32, i32> faces;
    };
    struct EdgeCountBufferType {
      u32 IndexCountPerInstance;
      u32 InstanceCount;
      u32 StartIndexLocation;
      i32 BaseVertexLocation;
      u32 StartInstanceLocation;
    };

    BufferRef EdgeBuffer;
    BufferRef EdgeCountBuffer;
    StructuredObjectViews Edge;
    StructuredObjectViews EdgeCount;

    explicit Buffers(ResourceAllocationContext &context, u32 MaxIndexCount);

    ~Buffers() = default;
  };

  struct Constants {
    u32 faceCount;
  };

  struct Inp {
    Buffers &buffers;
    GpuVirtualAddress lights;
    ImmutableMesh &mesh;
    MeshSpecificBuffers &meshBuffers;
  };

  RootSignature<ShaderMask> Signature;
  PipelineState pipeline;

  SilhouetteDetector(PipelineStateProvider &pipelineProvider,
                     GraphicsDevice &device, ComputeShader *cs);

  static SilhouetteDetector
  WithDefaultShaders(PipelineStateProvider &pipelineProvider,
                     GraphicsDevice &device);
  void Pre(CommandAllocator &allocator) const override {
    pipeline.Apply(allocator);
  }
  void Run(CommandAllocator &allocator, DynamicBufferManager &buffermanager,
           const Inp &inp) const;
  ~SilhouetteDetector() override = default;
};

struct SilhouetteClear : ShaderJob {
  struct ShaderMask : public RootSignatureMask {

    RootDescriptorTable<1> buff;

    explicit ShaderMask(const RootSignatureContext &context)
        : RootSignatureMask(context),

          buff(this, {DescriptorRangeType::UnorderedAccess, {0}}) {
      Flags = RootSignatureFlags::None;
    }
  };

  struct Inp {
    SilhouetteDetector::Buffers &buffers;
  };

  RootSignature<ShaderMask> Signature;
  PipelineState pipeline;

  SilhouetteClear(PipelineStateProvider &pipelineProvider,
                  GraphicsDevice &device, ComputeShader *cs);

  static SilhouetteClear
  WithDefaultShaders(PipelineStateProvider &pipelineProvider,
                     GraphicsDevice &device);
  void Pre(CommandAllocator &allocator) const override {
    pipeline.Apply(allocator);
  }
  void Run(CommandAllocator &allocator, DynamicBufferManager &buffermanager,
           const SilhouetteClear::Inp &inp) const;
  ~SilhouetteClear() override = default;
};

struct SilhouetteDetectorTester : ShaderJob {
  struct ShaderMask : public RootSignatureMask {
    static constexpr D3D_PRIMITIVE_TOPOLOGY Topology =
        D3D_PRIMITIVE_TOPOLOGY_LINELIST;

    RootDescriptorTable<1> Vertex;
    RootDescriptorTable<1> Edges;
    RootDescriptorTable<1> texture;
    RootDescriptor<RootDescriptorType::ConstantBuffer> camera;
    RootDescriptor<RootDescriptorType::ConstantBuffer> model;

    StaticSampler _textureSampler;

    explicit ShaderMask(const RootSignatureContext &context)
        : RootSignatureMask(context),

          Vertex(this, {DescriptorRangeType::ShaderResource, {0}}),
          Edges(this, {DescriptorRangeType::ShaderResource, {1}}),
          texture(this, {DescriptorRangeType::ShaderResource, {3}},
                  ShaderVisibility::Pixel),
          camera(this, {0}), model(this, {1}),
          _textureSampler(this, {0}, Filter::Linear, TextureAddressMode::Wrap,
                          ShaderVisibility::All)

    {
      Flags = RootSignatureFlags::None;
    }
  };

  using Buffers = SilhouetteDetector::Buffers;

  struct Inp {
    const GpuVirtualAddress &camera;
    const GpuVirtualAddress &modelTransform;
    const std::optional<GpuVirtualAddress> texture;
    const ImmutableMesh &mesh;
    Buffers &buffers;
    MeshSpecificBuffers &meshBuffers;
  };
  struct ModelConstants {
    XMFLOAT4X4 mMatrix;
  };

  winrt::com_ptr<ID3D12CommandSignature> indirectCommandSignature;

  RootSignature<ShaderMask> Signature;
  PipelineState pipeline;

  SilhouetteDetectorTester(PipelineStateProvider &pipelineProvider,
                           GraphicsDevice &device, VertexShader *vs,
                           PixelShader *ps);

  static SilhouetteDetectorTester
  WithDefaultShaders(PipelineStateProvider &pipelineProvider,
                     GraphicsDevice &device);
  void Pre(CommandAllocator &allocator) const override {
    pipeline.Apply(allocator);
  }
  void Run(CommandAllocator &allocator, DynamicBufferManager &buffermanager,
           const Inp &inp) const;
  ~SilhouetteDetectorTester() override = default;
};
