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
    RootDescriptorTable<1> HeightMap;

    RootDescriptor<RootDescriptorType::ConstantBuffer> ComputeConstants;

    // Out
    // Texture2D<float2>
    RootDescriptorTable<1> ConeMap;

    explicit ShaderMask(const RootSignatureContext &context)
        : RootSignatureMask(context), ComputeConstants(this, {9}),
          HeightMap(this, {DescriptorRangeType::ShaderResource, {0}}),
          ConeMap(this, {DescriptorRangeType::UnorderedAccess, {10}})

    {
      Flags = RootSignatureFlags::None;
    }
  };
  struct Inp {
    const UnorderedAccessView *const coneMap;
    // Currently float4
    const ShaderResourceView *const heightMap;
    const GpuVirtualAddress &ComputeConstants;
    const u32 N;
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
    std::array<const MutableTexture *const, 3> &coneMap;
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
