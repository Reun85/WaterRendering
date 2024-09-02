
#pragma once
#include "pch.h"

struct TextureBuffers {
  // Allocates necessary buffers if they are not yet allocated. May use the
  // finalTarget size to determine the sizes of the buffers
  virtual void MakeCompatible(const RenderTargetView &finalTarget,
                              ResourceAllocationContext &allocationContext) = 0;

  // Get ready for next frame
  virtual void Clear(CommandAllocator &allocator) = 0;
};

using namespace Axodox::Infrastructure;
using namespace Axodox::Storage;
using namespace DirectX::PackedVector;
struct WaterGraphicRootDescription : public RootSignatureMask {
  struct cameraConstants {
    XMFLOAT4X4 vMatrix;
    XMFLOAT4X4 pMatrix;
    XMFLOAT4X4 vpMatrix;
    XMFLOAT3 cameraPos;
  };
  struct ModelConstants {
    XMFLOAT4X4 mMatrix;
  };

  struct VertexConstants {
    struct InstanceData {
      XMFLOAT2 scaling;
      XMFLOAT2 offset;
    };
    InstanceData instanceData[Defaults::App::maxInstances];
  };
  struct HullConstants {
    struct InstanceData {
      XMFLOAT4 TesselationFactor;
    };
    // zneg,xneg, zpos, xpos
    InstanceData instanceData[Defaults::App::maxInstances];
  };

  struct PixelLighting {

    struct PixelLightData {
      XMFLOAT4 lightPos;   // .a 0 for directional, 1 for positional
      XMFLOAT4 lightColor; // .a is lightIntensity
      XMFLOAT4 AmbientColor;
    };
    std::array<PixelLightData, ShaderConstantCompat::maxLightCount> lights;
    int lightCount;

    static constexpr PixelLighting SunData() {
      PixelLighting data;
      data.lightCount = 1;
      data.lights[0].lightPos = XMFLOAT4(1, 0.109, 0.964, 0);
      data.lights[0].lightColor =
          XMFLOAT4(230.f / 255.f, 214.f / 255.f, 167.f / 255.f, 0.446);

      data.lights[0].AmbientColor =
          XMFLOAT4(53.f / 255.f, 111.f / 255.f, 111.f / 255.f, .639f);

      return data;
    }
  };

  struct PixelShaderPBRData {

    float3 SurfaceColor;
    float Roughness;
    float SubsurfaceScattering;
    float Sheen;
    float SheenTint;
    float Anisotropic;
    float SpecularStrength;
    float Metallic;
    float SpecularTint;
    float Clearcoat;
    float ClearcoatGloss;

    static constexpr PixelShaderPBRData WaterData() {
      PixelShaderPBRData data;
      data.SurfaceColor = float3(0.109f, 0.340f, 0.589f);
      data.Roughness = 0.285f;
      data.SubsurfaceScattering = 0.571f;
      data.Sheen = 0.163f;
      data.SheenTint = 0.366f;
      data.Anisotropic = 0.0f;
      data.SpecularStrength = 0.552f;
      data.Metallic = 0.0f;
      data.SpecularTint = 0.0f;
      data.Clearcoat = 0.157f;
      data.ClearcoatGloss = 0.323f;
      return data;
    }
  };

  struct NewPixelShaderData {

    float3 SurfaceColor = float3(0.109, 0.340, 0.589);
    float Roughness = 0.192;
    float3 _TipColor = float3(231.f, 231.f, 231.f) / 255.f;
    float foamDepthFalloff = 0.245;
    // float3 _ScatterColor = float3(4.f / 255.f, 22.f / 255.f, 33.f / 255.f);
    // float3 _ScatterColor = float3(10.f, 18.f, 29.f) / 255.f;
    // float3 _ScatterColor = float3(5.f, 18.f, 34.f) / 255.f;
    float3 _ScatterColor = float3(0.f, 11.f, 25.f) / 255.f;
    float foamRoughnessModifier = 5.0f;
    float3 _AmbientColor = float3(53, 111, 111) / 255.f;
    float _AmbientMult = 0.639f;
    float NormalDepthAttenuation = 1;
    float _HeightModifier = 60.f;
    float _WavePeakScatterStrength = 4.629f;
    float _ScatterStrength = 0.78f;
    float _ScatterShadowStrength = 4.460f;
  };

  RootDescriptor<RootDescriptorType::ConstantBuffer> vertexBuffer;
  RootDescriptor<RootDescriptorType::ConstantBuffer> hullBuffer;
  RootDescriptor<RootDescriptorType::ConstantBuffer> cameraBuffer;
  RootDescriptor<RootDescriptorType::ConstantBuffer> modelBuffer;
  RootDescriptor<RootDescriptorType::ConstantBuffer> debugBuffer;
  RootDescriptor<RootDescriptorType::ConstantBuffer> lightingBuffer;
  RootDescriptor<RootDescriptorType::ConstantBuffer> waterPBRBuffer;

  // Optional
  RootDescriptorTable<1> texture;

  RootDescriptorTable<1> heightMapHighest;
  RootDescriptorTable<1> heightMapMedium;
  RootDescriptorTable<1> heightMapLowest;
  RootDescriptorTable<1> gradientsHighest;
  RootDescriptorTable<1> gradientsMedium;
  RootDescriptorTable<1> gradientsLowest;

  StaticSampler _textureSampler;

  explicit WaterGraphicRootDescription(const RootSignatureContext &context)
      : RootSignatureMask(context),
        vertexBuffer(this, {2}, ShaderVisibility::Vertex),
        hullBuffer(this, {1}, ShaderVisibility::Hull),
        cameraBuffer(this, {0}, ShaderVisibility::All),
        modelBuffer(this, {1}, ShaderVisibility::Vertex),
        debugBuffer(this, {9}, ShaderVisibility::All),
        lightingBuffer(this, {1}, ShaderVisibility::Pixel),
        waterPBRBuffer(this, {2}, ShaderVisibility::Pixel),

        texture(this, {DescriptorRangeType::ShaderResource, {0}},
                ShaderVisibility::Pixel),
        heightMapHighest(this, {DescriptorRangeType::ShaderResource, {0}},
                         ShaderVisibility::Domain),
        heightMapMedium(this, {DescriptorRangeType::ShaderResource, {1}},
                        ShaderVisibility::Domain),
        heightMapLowest(this, {DescriptorRangeType::ShaderResource, {2}},
                        ShaderVisibility::Domain),
        gradientsHighest(this, {DescriptorRangeType::ShaderResource, {3}},
                         ShaderVisibility::Domain),
        gradientsMedium(this, {DescriptorRangeType::ShaderResource, {4}},
                        ShaderVisibility::Domain),
        gradientsLowest(this, {DescriptorRangeType::ShaderResource, {5}},
                        ShaderVisibility::Domain),
        _textureSampler(this, {0}, Filter::Linear, TextureAddressMode::Wrap,
                        ShaderVisibility::All) {
    Flags = RootSignatureFlags::AllowInputAssemblerInputLayout;
  }
};

struct DeferredShading : public RootSignatureMask {

  struct GBuffer : TextureBuffers {
    MutableTexture Albedo;
    MutableTexture Position;
    MutableTexture Normal;
    MutableTexture MaterialValues;

    std::array<const RenderTargetView *, 4> GetGBufferViews() const {
      return {Albedo.RenderTarget(), Normal.RenderTarget(),
              Position.RenderTarget(), MaterialValues.RenderTarget()};
    }

    //
    constexpr static std::array<Format, 4> GetGBufferFormats() {
      return {Format::B8G8R8A8_UNorm, Format::R16G16B16A16_Float,
              Format::R16G16B16A16_Float, Format::B8G8R8A8_UNorm};
    };

    explicit GBuffer(const ResourceAllocationContext &context)
        : Albedo(context), Position(context), Normal(context),
          MaterialValues(context) {}

    void MakeCompatible(const RenderTargetView &finalTarget,
                        ResourceAllocationContext &allocationContext) override {
      auto UpdateTexture = [&finalTarget](MutableTexture &texture,
                                          const Format &format,
                                          const TextureFlags &flags) {
        if (!texture || !TextureDefinition::AreSizeCompatible(
                            *texture.Definition(), finalTarget.Definition())) {
          texture.Reset();
          auto definition =
              finalTarget.Definition().MakeSizeCompatible(format, flags);
          texture.Allocate(definition);
        }
      };
      UpdateTexture(Albedo, Format::B8G8R8A8_UNorm, TextureFlags::RenderTarget);
      UpdateTexture(Position, Format::R16G16B16A16_Float,
                    TextureFlags::RenderTarget);
      UpdateTexture(Normal, Format::R16G16B16A16_Float,
                    TextureFlags::RenderTarget);
      UpdateTexture(MaterialValues, Format::B8G8R8A8_UNorm,
                    TextureFlags::RenderTarget);
    }

    void Clear(CommandAllocator &allocator) override {
      Albedo.RenderTarget()->Clear(allocator);
      Normal.RenderTarget()->Clear(allocator);
      Position.RenderTarget()->Clear(allocator);
      MaterialValues.RenderTarget()->Clear(allocator);
    }
    void TranslateToTarget(CommandAllocator &allocator) {
      allocator.TransitionResources({

          {Albedo.operator Axodox::Graphics::D3D12::ResourceArgument(),
           ResourceStates::PixelShaderResource, ResourceStates::RenderTarget},
          {Normal.operator Axodox::Graphics::D3D12::ResourceArgument(),
           ResourceStates::PixelShaderResource, ResourceStates::RenderTarget},
          {Position.operator Axodox::Graphics::D3D12::ResourceArgument(),
           ResourceStates::PixelShaderResource, ResourceStates::RenderTarget},
          {MaterialValues.operator Axodox::Graphics::D3D12::ResourceArgument(),
           ResourceStates::PixelShaderResource, ResourceStates::RenderTarget},
      });
    }
    void TranslateToView(CommandAllocator &allocator) {
      allocator.TransitionResources({
          {Albedo.operator Axodox::Graphics::D3D12::ResourceArgument(),
           ResourceStates::RenderTarget, ResourceStates::PixelShaderResource},
          {Normal.operator Axodox::Graphics::D3D12::ResourceArgument(),
           ResourceStates::RenderTarget, ResourceStates::PixelShaderResource},
          {Position.operator Axodox::Graphics::D3D12::ResourceArgument(),
           ResourceStates::RenderTarget, ResourceStates::PixelShaderResource},
          {MaterialValues.operator Axodox::Graphics::D3D12::ResourceArgument(),
           ResourceStates::RenderTarget, ResourceStates::PixelShaderResource},
      });
    }
  };

  RootDescriptorTable<1> albedo;
  RootDescriptorTable<1> normal;
  RootDescriptorTable<1> position;
  RootDescriptorTable<1> materialValues;
  RootDescriptorTable<1> geometryDepth;
  RootDescriptorTable<1> skybox;

  RootDescriptor<RootDescriptorType::ConstantBuffer> lightingBuffer;
  RootDescriptor<RootDescriptorType::ConstantBuffer> cameraBuffer;
  RootDescriptor<RootDescriptorType::ConstantBuffer> debugBuffer;
  StaticSampler Sampler;

  explicit DeferredShading(const RootSignatureContext &context)
      : RootSignatureMask(context),
        albedo(this, {DescriptorRangeType::ShaderResource, 0}),
        normal(this, {DescriptorRangeType::ShaderResource, 1}),
        position(this, {DescriptorRangeType::ShaderResource, 2}),
        materialValues(this, {DescriptorRangeType::ShaderResource, 3}),
        geometryDepth(this, {DescriptorRangeType::ShaderResource, 4}),
        skybox(this, {DescriptorRangeType::ShaderResource, {5}},
               ShaderVisibility::Pixel),
        lightingBuffer(this, {1}, ShaderVisibility::Pixel),
        cameraBuffer(this, {0}, ShaderVisibility::Pixel),
        debugBuffer(this, {9}, ShaderVisibility::Pixel),
        Sampler(this, {0}, Filter::Linear, TextureAddressMode::Clamp) {

    Flags = RootSignatureFlags::AllowInputAssemblerInputLayout;
  }

  void BindGBuffer(const GBuffer &buffers) {
    albedo = *buffers.Albedo.ShaderResource();
    normal = *buffers.Normal.ShaderResource();
    position = *buffers.Position.ShaderResource();
    materialValues = *buffers.MaterialValues.ShaderResource();
  }
};

struct ShadowMapping : public RootSignatureMask {

  struct Buffers : TextureBuffers {
    MutableTextureWithViews DepthBuffer;

    explicit Buffers(const ResourceAllocationContext &context,
                     const u32 N = 1024)

        : DepthBuffer(
              context,
              TextureDefinition(Format::D32_Float, N, N, 0,
                                TextureFlags::ShaderResourceDepthStencil),
              TextureViewDefinitions::GetDepthStencilWithShaderView(
                  Format::D32_Float, Format::R32_Float)) {}

    void MakeCompatible(const RenderTargetView &finalTarget,
                        ResourceAllocationContext &allocationContext) override {
      // No need to allocate
    }
    void Clear(CommandAllocator &allocator) override {
      DepthBuffer.DepthStencil()->Clear(allocator);
    }
    void TranslateToTarget(CommandAllocator &allocator) {
      allocator.TransitionResources({

          {DepthBuffer.operator Axodox::Graphics::D3D12::ResourceArgument(),
           ResourceStates::PixelShaderResource, ResourceStates::DepthWrite},
      });
    }
    void TranslateToView(
        CommandAllocator &allocator,
        const ResourceStates &newState = ResourceStates::AllShaderResource) {
      allocator.TransitionResources({
          {DepthBuffer.operator Axodox::Graphics::D3D12::ResourceArgument(),
           ResourceStates::DepthWrite, newState},
      });
    }
  };

  RootDescriptorTable<1> lightBuffer;

  explicit ShadowMapping(const RootSignatureContext &context)
      : RootSignatureMask(context),
        lightBuffer(this, {DescriptorRangeType::ShaderResource, 0}) {

    Flags = RootSignatureFlags::AllowInputAssemblerInputLayout;
  }
};

struct FrameResources : TextureBuffers {
  CommandAllocator Allocator;
  CommandFence Fence;
  CommandFenceMarker Marker;
  DynamicBufferManager DynamicBuffer;

  MutableTextureWithViews DepthBuffer;
  descriptor_ptr<ShaderResourceView> ScreenResourceView;
  DeferredShading::GBuffer GBuffer;

  void MakeCompatible(const RenderTargetView &finalTarget,
                      ResourceAllocationContext &allocationContext) override {
    // Ensure depth buffer matches frame size
    if (!DepthBuffer ||
        !TextureDefinition::AreSizeCompatible(*DepthBuffer.Definition(),
                                              finalTarget.Definition())) {

      auto depthDefinition = finalTarget.Definition().MakeSizeCompatible(
          Format::D32_Float, TextureFlags::ShaderResourceDepthStencil);

      auto depthViews = TextureViewDefinitions::GetDepthStencilWithShaderView(
          Format::D32_Float, Format::R32_Float);

      DepthBuffer.Allocate(depthDefinition, depthViews);
    }

    // Ensure screen shader resource view
    if (!ScreenResourceView ||
        ScreenResourceView->Resource() != finalTarget.Resource()) {
      Texture screenTexture{finalTarget.Resource()};
      ScreenResourceView =
          allocationContext.CommonDescriptorHeap->CreateShaderResourceView(
              &screenTexture);
    }

    // Ensure GBuffer compatibility
    GBuffer.MakeCompatible(finalTarget, allocationContext);
  }

  void Clear(CommandAllocator &allocator) override {

    // Clears frame with background color
    DepthBuffer.DepthStencil()->Clear(allocator);
    GBuffer.Clear(allocator);
  }

  explicit FrameResources(const ResourceAllocationContext &context)
      : Allocator(*context.Device), Fence(*context.Device),
        DynamicBuffer(*context.Device), DepthBuffer(context), GBuffer(context) {
  }
};
