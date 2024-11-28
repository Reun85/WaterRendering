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

struct ConeMapCreater2 : ShaderJob {
  struct ShaderMask : public RootSignatureMask {
    // In
    // Texture2D<float4>
    // should change to float
    RootDescriptorTable<1> MixMax;

    RootDescriptor<RootDescriptorType::ConstantBuffer> ComputeConstants;

    // Out
    // Texture2D<float2>
    RootDescriptorTable<1> ConeMap;

    explicit ShaderMask(const RootSignatureContext &context)
        : RootSignatureMask(context),
          MixMax(this, {DescriptorRangeType::ShaderResource, {0}}),
          ComputeConstants(this, {0}),
          ConeMap(this, {DescriptorRangeType::UnorderedAccess, {0}})

    {
      Flags = RootSignatureFlags::None;
    }
    struct Constants {
      u32 maxLevel;        // ending mipmap level
      XMUINT2 textureSize; // texture size
      u32 padding = 0;
      XMFLOAT2 deltaHalf; // (UV size of a texel)/2
    };
  };
  struct Inp {
    const UnorderedAccessView *const coneMap;
    // Currently float4
    const ShaderResourceView *const mixMax;
    const u32 N;
  };

  RootSignature<ShaderMask> Signature;
  PipelineState pipeline;

  ConeMapCreater2(PipelineStateProvider &pipelineProvider,
                  GraphicsDevice &device, ComputeShader *cs);

  static ConeMapCreater2
  WithDefaultShaders(PipelineStateProvider &pipelineProvider,
                     GraphicsDevice &device);
  void Pre(CommandAllocator &allocator) const override {
    pipeline.Apply(allocator);
  }
  void Run(CommandAllocator &allocator, DynamicBufferManager &buffermanager,
           const Inp &inp) const;
  ~ConeMapCreater2() override = default;
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
      Flags = RootSignatureFlags::AllowInputAssemblerInputLayout;
    }
  };

  struct ModelBuffers {
    float3 center;
    int padding = 0;
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
    ImmutableMesh &mesh;
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

struct PrismParallaxDraw : ShaderJob {
  struct ShaderMask : public RootSignatureMask {

    RootDescriptor<RootDescriptorType::ConstantBuffer> cameraBuffer;
    RootDescriptor<RootDescriptorType::ConstantBuffer> modelBuffer;
    RootDescriptor<RootDescriptorType::ConstantBuffer> vertexBuffer;
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

    RootDescriptorTable<1> minMaxHighest;
    RootDescriptorTable<1> minMaxMedium;
    RootDescriptorTable<1> minMaxLowest;

    StaticSampler _textureSampler;

    explicit ShaderMask(const RootSignatureContext &context)
        : RootSignatureMask(context),
          cameraBuffer(this, {0}, ShaderVisibility::All),
          modelBuffer(this, {1}, ShaderVisibility::All),
          vertexBuffer(this, {2}, ShaderVisibility::All),
          debugBuffer(this, {9}, ShaderVisibility::All),
          waterPBRBuffer(this, {3}, ShaderVisibility::Pixel),

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
          minMaxHighest(this, {DescriptorRangeType::ShaderResource, {6}},
                        ShaderVisibility::Vertex),
          minMaxMedium(this, {DescriptorRangeType::ShaderResource, {7}},
                       ShaderVisibility::Vertex),
          minMaxLowest(this, {DescriptorRangeType::ShaderResource, {8}},
                       ShaderVisibility::Vertex),
          _textureSampler(this, {0}, Filter::Linear, TextureAddressMode::Wrap,
                          ShaderVisibility::All) {
      Flags = RootSignatureFlags::AllowInputAssemblerInputLayout;
    }
  };

  struct ModelBuffers {
    XMFLOAT4X4 mMatrix;
    XMFLOAT4X4 mINVMatrix;
    XMFLOAT3 center;

    float PrismHeight;
    GpuVirtualAddress Upload(DynamicBufferManager &bufferManager) {
      return bufferManager.AddBuffer(this);
    }
  };
  struct VertexData {
    struct InstanceData {
      XMFLOAT2 scaling;
      XMFLOAT2 offset;
    };
    std::array<InstanceData, DefaultsValues::App::maxInstances> instanceData;
    GpuVirtualAddress Upload(DynamicBufferManager &bufferManager) {
      return bufferManager.AddBuffer(this);
    }
  };

  struct Inp {
    const std::array<ShaderResourceView *const, 3> &coneMaps;
    const std::array<ShaderResourceView *const, 3> &gradients;
    const std::array<ShaderResourceView *const, 3> &minMax;
    std::optional<ShaderResourceView *const> texture;
    GpuVirtualAddress cameraBuffer;
    GpuVirtualAddress debugBuffers;
    GpuVirtualAddress waterPBRBuffers;
    GpuVirtualAddress modelBuffers;
    ImmutableMesh &mesh;
    GpuVirtualAddress vertexData;
    u32 N;
  };

  RootSignature<ShaderMask> Signature;
  PipelineState pipeline;

  PrismParallaxDraw(PipelineStateProvider &pipelineProvider,
                    GraphicsDevice &device, VertexShader *vs, PixelShader *ps);

  static PrismParallaxDraw
  WithDefaultShaders(PipelineStateProvider &pipelineProvider,
                     GraphicsDevice &device);
  void Pre(CommandAllocator &allocator) const override {
    pipeline.Apply(allocator);
  }
  void Run(CommandAllocator &allocator, const Inp &inp) const;
  ~PrismParallaxDraw() override = default;
};

struct MixMaxCompute : ShaderJob {
  struct Buffer {

    u32 SrcMipLevel;  // Texture level of source mip
    u32 NumMipLevels; // Number of OutMips to write: [1, 4]
    float2 TexelSize; // 1.0 / OutMip1.Dimensions
  };
  struct ShaderMask : public RootSignatureMask {
    // In
    // Texture2D<float4>
    // should change to float
    RootDescriptorTable<1> ReadTexture;

    RootDescriptor<RootDescriptorType::ConstantBuffer> ComputeConstants;

    // Out
    // Texture2D<float4>
    RootDescriptorTable<1> MipMap1;
    RootDescriptorTable<1> MipMap2;
    RootDescriptorTable<1> MipMap3;
    RootDescriptorTable<1> MipMap4;

    StaticSampler _textureSampler;

    explicit ShaderMask(const RootSignatureContext &context)
        : RootSignatureMask(context),
          ReadTexture(this, {DescriptorRangeType::ShaderResource, {0}}),
          ComputeConstants(this, {0}),
          MipMap1(this, {DescriptorRangeType::UnorderedAccess, 0}),
          MipMap2(this, {DescriptorRangeType::UnorderedAccess, 1}),
          MipMap3(this, {DescriptorRangeType::UnorderedAccess, 2}),
          MipMap4(this, {DescriptorRangeType::UnorderedAccess, 3}),
          _textureSampler(this, {0}, Filter::Linear, TextureAddressMode::Clamp,
                          ShaderVisibility::All) {
      Flags = RootSignatureFlags::None;
    }
  };
  struct Inp {
    // Mip level 0 should be set by somewhere else
    const u32 mipLevels;
    const u32 Extent;
    ShaderResourceView *texture;
    std::array<UnorderedAccessView *, 4> mipMaps;
  };

  RootSignature<ShaderMask> Signature;
  PipelineState pipeline;

  MixMaxCompute(PipelineStateProvider &pipelineProvider, GraphicsDevice &device,
                ComputeShader *cs);

  static MixMaxCompute
  WithDefaultShaders(PipelineStateProvider &pipelineProvider,
                     GraphicsDevice &device);
  void Pre(CommandAllocator &allocator) const override {
    pipeline.Apply(allocator);
  }
  void Run(CommandAllocator &allocator, DynamicBufferManager &buffermanager,
           const Inp &inp) const;
  ~MixMaxCompute() override = default;
};

struct DisplacedHeightMapJob : ShaderJob {
  struct ShaderMask : public RootSignatureMask {
    // In
    // Texture2D<float4>
    // should change to float
    // float3
    RootDescriptorTable<1> DisplacementMap;
    // float4
    RootDescriptorTable<1> Gradients;

    RootDescriptor<RootDescriptorType::ConstantBuffer> ComputeConstants;

    // Out
    // float
    RootDescriptorTable<1> OutHeightMap;
    // float4
    RootDescriptorTable<1> OutGradients;

    explicit ShaderMask(const RootSignatureContext &context)
        : RootSignatureMask(context),
          DisplacementMap(this, {DescriptorRangeType::ShaderResource, {0}}),
          Gradients(this, {DescriptorRangeType::ShaderResource, {1}}),
          ComputeConstants(this, {0}),
          OutHeightMap(this, {DescriptorRangeType::UnorderedAccess, 0}),
          OutGradients(this, {DescriptorRangeType::UnorderedAccess, 1}) {
      Flags = RootSignatureFlags::None;
    }
  };
  struct Inp {
    // Mip level 0 should be set by somewhere else
    const GpuVirtualAddress ComputeConstants;
    ShaderResourceView *displacementMap;
    ShaderResourceView *gradients;
    UnorderedAccessView *outHeightMap;
    UnorderedAccessView *outGradients;
    /// <summary>
    /// texture size
    /// </summary>
    const u32 N;
  };

  RootSignature<ShaderMask> Signature;
  PipelineState pipeline;

  DisplacedHeightMapJob(PipelineStateProvider &pipelineProvider,
                        GraphicsDevice &device, ComputeShader *cs);

  static DisplacedHeightMapJob
  WithDefaultShaders(PipelineStateProvider &pipelineProvider,
                     GraphicsDevice &device);
  void Pre(CommandAllocator &allocator) const override {
    pipeline.Apply(allocator);
  }
  void Run(CommandAllocator &allocator, DynamicBufferManager &buffermanager,
           const Inp &inp) const;
  ~DisplacedHeightMapJob() override = default;
};
