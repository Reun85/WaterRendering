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

namespace SimulationStage {
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
        : RootSignatureMask(context),
          HeightMap(this, {DescriptorRangeType::ShaderResource, {0}}),
          ComputeConstants(this, {9}),
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

} // namespace SimulationStage

struct ParallaxDraw : ShaderJob {
  struct ShaderMask : public RootSignatureMask {

    RootDescriptor<RootDescriptorType::ConstantBuffer> cameraBuffer;
    RootDescriptor<RootDescriptorType::ConstantBuffer> modelBuffer;
    RootDescriptor<RootDescriptorType::ConstantBuffer> debugBuffer;
    RootDescriptor<RootDescriptorType::ConstantBuffer> waterPBRBuffer;

    // Optional
    RootDescriptorTable<1> texture;

    RootDescriptorTable<1> coneMapHighest;
    RootDescriptorTable<1> coneMapMedium;
    RootDescriptorTable<1> coneMapLowest;
    RootDescriptorTable<1> gradientsHighest;
    RootDescriptorTable<1> gradientsMedium;
    RootDescriptorTable<1> gradientsLowest;

    StaticSampler _textureSampler;

    explicit ShaderMask(const RootSignatureContext &context)
        : RootSignatureMask(context),
          cameraBuffer(this, {0}, ShaderVisibility::All),
          modelBuffer(this, {1}, ShaderVisibility::All),
          debugBuffer(this, {9}, ShaderVisibility::All),
          waterPBRBuffer(this, {2}, ShaderVisibility::Pixel),

          texture(this, {DescriptorRangeType::ShaderResource, {9}},
                  ShaderVisibility::Pixel),
          coneMapHighest(this, {DescriptorRangeType::ShaderResource, {0}},
                         ShaderVisibility::Pixel),
          coneMapMedium(this, {DescriptorRangeType::ShaderResource, {1}},
                        ShaderVisibility::Pixel),
          coneMapLowest(this, {DescriptorRangeType::ShaderResource, {2}},
                        ShaderVisibility::Pixel),
          gradientsHighest(this, {DescriptorRangeType::ShaderResource, {3}},
                           ShaderVisibility::Pixel),
          gradientsMedium(this, {DescriptorRangeType::ShaderResource, {4}},
                          ShaderVisibility::Pixel),
          gradientsLowest(this, {DescriptorRangeType::ShaderResource, {5}},
                          ShaderVisibility::Pixel),
          _textureSampler(this, {0}, Filter::Linear, TextureAddressMode::Wrap,
                          ShaderVisibility::All) {
      Flags = RootSignatureFlags::None;
    }
  };

  struct ModelBuffers {
    float3 center;
    float2 scale;
    GpuVirtualAddress Upload(DynamicBufferManager &bufferManager) {
      return bufferManager.AddBuffer(this);
    }
  };

  struct Inp {
    const std::array<ShaderResourceView *const, 3> &coneMaps;
    const std::array<ShaderResourceView *const, 3> &gradients;
    std::optional<ShaderResourceView *const> texture;
    GpuVirtualAddress modelBuffers;
    GpuVirtualAddress cameraBuffer;
    GpuVirtualAddress debugBuffers;
    GpuVirtualAddress waterPBRBuffers;
  };

  RootSignature<ShaderMask> Signature;
  PipelineState pipeline;

  ParallaxDraw(PipelineStateProvider &pipelineProvider, GraphicsDevice &device,
               VertexShader *vs, PixelShader *ps);

  static ParallaxDraw
  WithDefaultShaders(PipelineStateProvider &pipelineProvider,
                     GraphicsDevice &device);
  void Pre(CommandAllocator &allocator) const override {
    pipeline.Apply(allocator);
  }
  void Run(CommandAllocator &allocator, const Inp &inp) const;
  ~ParallaxDraw() override = default;
};
