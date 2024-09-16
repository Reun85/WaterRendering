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

          Vertex(this, {DescriptorRangeType::ShaderResource, 0}),
          Index(this, {DescriptorRangeType::ShaderResource, 1}),
          Edges(this, {DescriptorRangeType::UnorderedAccess, 0}),
          EdgeCount(this, {DescriptorRangeType::UnorderedAccess, 1}),
          lights(this, {1}), constants(this, {0})

    {
      Flags = RootSignatureFlags::None;
    }
  };

  struct Buffers : ShaderBuffers {
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

    explicit Buffers(const ResourceAllocationContext &context,
                     u32 MaxIndexCount);

    void MakeCompatible(const RenderTargetView &finalTarget,
                        ResourceAllocationContext &allocationContext) override {
      // Nothing to do
    }
    void Clear(CommandAllocator &allocator) override {}
    ~Buffers() override = default;
  };

  struct Constants {
    u32 faceCount;
  };

  struct Inp {
    Buffers &buffers;
    GpuVirtualAddress lights;
    ImmutableMesh &mesh;
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

struct SilhouetteClearTask : ShaderJob {
  struct ShaderMask : public RootSignatureMask {

    struct Constants {
      u32 BytesTimes4;
    };

    RootDescriptorTable<1> buff;

    RootDescriptor<RootDescriptorType::ConstantBuffer> constants;

    explicit ShaderMask(const RootSignatureContext &context)
        : RootSignatureMask(context),

          buff(this, {DescriptorRangeType::UnorderedAccess, 0}),
          constants(this, {0})

    {
      Flags = RootSignatureFlags::None;
    }
  };

  struct Inp {
    SilhouetteDetector::Buffers buffer;
  };

  RootSignature<ShaderMask> Signature;
  PipelineState pipeline;

  SilhouetteClearTask(PipelineStateProvider &pipelineProvider,
                      GraphicsDevice &device, ComputeShader *cs);

  static SilhouetteClearTask
  WithDefaultShaders(PipelineStateProvider &pipelineProvider,
                     GraphicsDevice &device);
  void Pre(CommandAllocator &allocator) const override {
    pipeline.Apply(allocator);
  }
  void Run(CommandAllocator &allocator, DynamicBufferManager &buffermanager,
           const Inp &inp) const;
  ~SilhouetteClearTask() override = default;
};

struct SilhouetteDetectorTester : ShaderJob {

  struct ShaderMask : public RootSignatureMask {

    RootDescriptorTable<1> Vertex;
    RootDescriptorTable<1> Edges;

    explicit ShaderMask(const RootSignatureContext &context)
        : RootSignatureMask(context),

          Vertex(this, {DescriptorRangeType::ShaderResource, 0}),
          Edges(this, {DescriptorRangeType::ShaderResource, 1})

    {
      Flags = RootSignatureFlags::None;
    }
  };

  using Buffers = SilhouetteDetector::Buffers;

  struct Inp {
    Buffers &buffers;
  };

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
