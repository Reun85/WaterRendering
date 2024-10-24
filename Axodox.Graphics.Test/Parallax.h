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

struct ConeMapCreater : ShaderJob {
  struct ShaderMask : public RootSignatureMask {
    // In
    // Texture2D<float4>
    // should change to float
    RootDescriptor<RootDescriptorType::ShaderResource> HeightMap;

    // Out
    // Texture2D<float2>
    RootDescriptor<RootDescriptorType::UnorderedAccess> ConeMap;

    explicit ShaderMask(const RootSignatureContext &context)
        : RootSignatureMask(context), HeightMap(this, {0}), ConeMap(this, {10})

    {
      Flags = RootSignatureFlags::None;
    }
  };

  struct Textures : ShaderBuffers {
    MutableTextureWithViews ConeMap;

    explicit Textures(const ResourceAllocationContext &context);

    void MakeCompatible(const RenderTargetView &finalTarget,
                        ResourceAllocationContext &allocationContext) override {
      // No need to allocate
    }
    void Clear(CommandAllocator &allocator) override;
    void TranslateToInp(CommandAllocator &allocator);
    void TranslateToOutput(CommandAllocator &allocator);
    ~Textures() override = default;
  };
  struct Buffers {

    explicit Buffers(ResourceAllocationContext &context);

    ~Buffers() = default;
  };

  struct Inp {
    Textures textureBuffer;
    Buffer buffers;
    // impls ShaderResource();
    MutableTexture *heightMap;
  };

  RootSignature<ShaderMask> Signature;
  PipelineState pipeline;

  ConeMapCreater(PipelineStateProvider &pipelineProvider,
                 GraphicsDevice &device, ComputeShader *cs);

  static ConeMapCreater
  WithDefaultShaders(PipelineStateProvider &pipelineProvider,
                     GraphicsDevice &device);
  void Pre(CommandAllocator &allocator) const override {
    pipeline.Apply(allocator);
  }
  void Run(CommandAllocator &allocator, DynamicBufferManager &buffermanager,
           const Inp &inp) const;
  ~ConeMapCreater() override = default;
};

struct ParallaxDraw : ShaderJob {
  struct ShaderMask : public RootSignatureMask {

    RootDescriptorTable<1> buff;

    explicit ShaderMask(const RootSignatureContext &context)
        : RootSignatureMask(context),

          buff(this, {DescriptorRangeType::UnorderedAccess, {0}}) {
      Flags = RootSignatureFlags::None;
    }
  };

  struct Inp {
    std::array<ConeMapCreater::Textures, 3> &textures;
  };

  RootSignature<ShaderMask> Signature;
  PipelineState pipeline;

  ParallaxDraw(PipelineStateProvider &pipelineProvider, GraphicsDevice &device,
               VertexShader *vs, HullShader *hs, DomainShader *ds,
               PixelShader *ps);

  static ParallaxDraw
  WithDefaultShaders(PipelineStateProvider &pipelineProvider,
                     GraphicsDevice &device);
  void Pre(CommandAllocator &allocator) const override {
    pipeline.Apply(allocator);
  }
  void Run(CommandAllocator &allocator, DynamicBufferManager &buffermanager,
           const Inp &inp) const;
  ~ParallaxDraw() override = default;
};
