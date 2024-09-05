
#pragma once
#include "pch.h"
#include "Defaults.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Storage;
using namespace DirectX::PackedVector;
class Camera;

struct LightData {
  XMFLOAT4 lightPos;   // .a 0 for directional, 1 for positional
  XMFLOAT4 lightColor; // .a is lightIntensity
  XMFLOAT4 AmbientColor;
};

struct TextureBuffers {
  // Allocates necessary buffers if they are not yet allocated. May use the
  // finalTarget size to determine the sizes of the buffers
  virtual void MakeCompatible(const RenderTargetView &finalTarget,
                              ResourceAllocationContext &allocationContext) = 0;

  // Get ready for next frame
  virtual void Clear(CommandAllocator &allocator) = 0;

  ~TextureBuffers() {}
};
struct WaterGraphicRootDescription : public RootSignatureMask {

  struct cameraConstants {
    XMFLOAT4X4 vMatrix;
    XMFLOAT4X4 pMatrix;
    XMFLOAT4X4 vpMatrix;
    XMFLOAT4X4 INVvMatrix;
    XMFLOAT4X4 INVpMatrix;
    XMFLOAT4X4 INVvpMatrix;
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
    InstanceData instanceData[DefaultsValues::App::maxInstances];
  };
  struct HullConstants {
    struct InstanceData {
      XMFLOAT4 TesselationFactor;
    };
    // zneg,xneg, zpos, xpos
    InstanceData instanceData[DefaultsValues::App::maxInstances];
  };

  struct PixelLighting {

    std::array<LightData, ShaderConstantCompat::maxLightCount> lights;
    int lightCount;

    static constexpr PixelLighting SunData() {
      PixelLighting data;
      data.lightCount = 1;
      data.lights[0].lightPos = XMFLOAT4(1, 0.109, 0.964, 0);
      data.lights[0].lightColor =
          XMFLOAT4(234.f / 255.f, 204.f / 255.f, 118.f / 255.f, 0.446);

      data.lights[0].AmbientColor =
          XMFLOAT4(15.f / 255.f, 14.f / 255.f, 5.f / 255.f, .185);

      return data;
    }
    // old
  private:
    static constexpr PixelLighting old() {
      PixelLighting data;
      data.lightCount = 1;
      data.lights[0].lightPos = XMFLOAT4(1, 0.109, 0.964, 0);
      data.lights[0].lightColor =
          XMFLOAT4(243.f / 255.f, 206.f / 255.f, 97.f / 255.f, 0.446);

      data.lights[0].AmbientColor =
          XMFLOAT4(15.f / 255.f, 14.f / 255.f, 5.f / 255.f, .639f);

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

  struct WaterPixelShaderData {

    float3 AlbedoColor = float3(11.f, 53.f, 108.f) / 255.f;
    float Roughness = 0.192;
    float3 _TipColor = float3(231.f, 231.f, 231.f) / 255.f;
    float foamDepthFalloff = 0.245;
    float foamRoughnessModifier = 5.0f;
    float NormalDepthAttenuation = 1;
    float _HeightModifier = 1.871f;
    float _WavePeakScatterStrength = 4.629f;
    float _ScatterShadowStrength = 4.067f;
  };

  RootDescriptor<RootDescriptorType::ConstantBuffer> vertexBuffer;
  RootDescriptor<RootDescriptorType::ConstantBuffer> hullBuffer;
  RootDescriptor<RootDescriptorType::ConstantBuffer> cameraBuffer;
  RootDescriptor<RootDescriptorType::ConstantBuffer> modelBuffer;
  RootDescriptor<RootDescriptorType::ConstantBuffer> debugBuffer;
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

    const static constexpr u8 NumberOfBuffers = 4;

    std::array<const RenderTargetView *, NumberOfBuffers>
    GetGBufferViews() const {
      return {Albedo.RenderTarget(), Normal.RenderTarget(),
              Position.RenderTarget(), MaterialValues.RenderTarget()};
    }

    //
    constexpr static std::array<Format, NumberOfBuffers> GetGBufferFormats() {
      return {Format::B8G8R8A8_UNorm, Format::R16G16B16A16_Float,
              Format::R16G16B16A16_Float, Format::R16G16B16A16_Float};
    };

    std::array<MutableTexture *, NumberOfBuffers> GetBuffers() {
      return {&Albedo, &Normal, &Position, &MaterialValues};
    }
    std::array<std::pair<MutableTexture *, Format>, NumberOfBuffers>
    GetBuffersAndFormats();

    explicit GBuffer(const ResourceAllocationContext &context)
        : Albedo(context), Position(context), Normal(context),
          MaterialValues(context) {}

    void MakeCompatible(const RenderTargetView &finalTarget,
                        ResourceAllocationContext &allocationContext) override;

    void Clear(CommandAllocator &allocator) override;
    void TranslateToTarget(CommandAllocator &allocator);
    void TranslateToView(CommandAllocator &allocator);
  };

  struct DeferredShaderBuffers {
    float EnvMapMult = 1.f;
  };

  RootDescriptorTable<1> albedo;
  RootDescriptorTable<1> normal;
  RootDescriptorTable<1> position;
  RootDescriptorTable<1> materialValues;
  RootDescriptorTable<1> geometryDepth;
  RootDescriptorTable<1> skybox;

  // Water
  RootDescriptorTable<1> gradientsHighest;
  RootDescriptorTable<1> gradientsMedium;
  RootDescriptorTable<1> gradientsLowest;

  RootDescriptor<RootDescriptorType::ConstantBuffer> lightingBuffer;
  RootDescriptor<RootDescriptorType::ConstantBuffer> cameraBuffer;
  RootDescriptor<RootDescriptorType::ConstantBuffer> debugBuffer;
  RootDescriptor<RootDescriptorType::ConstantBuffer> deferredShaderBuffer;
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
        gradientsHighest(this, {DescriptorRangeType::ShaderResource, 20},
                         ShaderVisibility::Pixel),
        gradientsMedium(this, {DescriptorRangeType::ShaderResource, 21},
                        ShaderVisibility::Pixel),
        gradientsLowest(this, {DescriptorRangeType::ShaderResource, 22},
                        ShaderVisibility::Pixel),
        lightingBuffer(this, {1}, ShaderVisibility::Pixel),
        cameraBuffer(this, {0}, ShaderVisibility::Pixel),
        debugBuffer(this, {9}, ShaderVisibility::Pixel),
        deferredShaderBuffer(this, {2}, ShaderVisibility::Pixel),
        Sampler(this, {0}, Filter::Linear, TextureAddressMode::Clamp) {

    Flags = RootSignatureFlags::AllowInputAssemblerInputLayout;
  }

  void BindGBuffer(const GBuffer &buffers);
};

struct ShadowMapping : public RootSignatureMask {
  constexpr static size_t LODCOUNT = 3;

  struct Textures : TextureBuffers {
    MutableTextureWithViews DepthBuffer;

    explicit Textures(const ResourceAllocationContext &context,
                      const u32 N = 1024);

    void MakeCompatible(const RenderTargetView &finalTarget,
                        ResourceAllocationContext &allocationContext) override {
      // No need to allocate
    }
    void Clear(CommandAllocator &allocator) override;
    void TranslateToTarget(CommandAllocator &allocator);
    void TranslateToView(
        CommandAllocator &allocator,
        const ResourceStates &newState = ResourceStates::AllShaderResource);
  };

  struct Buffer {
    struct LOD {
      XMMATRIX viewProj;
      f32 farPlane;
    };

    std::array<LOD, LODCOUNT> lods;

    constexpr static f32 zMult = 10.f;

    void Update(const Camera &cam, const LightData &light);
    explicit Buffer(const Camera &cam, f32 closestFrustumEnd = 200.f,
                    f32 midFrustumEnd = 600.f);
  };

  RootDescriptorTable<LODCOUNT> lightBuffer;

  explicit ShadowMapping(const RootSignatureContext &context)
      : RootSignatureMask(context),
        lightBuffer(this, {{{DescriptorRangeType::ShaderResource, 0},
                            {DescriptorRangeType::ShaderResource, 1},
                            {DescriptorRangeType::ShaderResource, 2}}}) {

    Flags = RootSignatureFlags::AllowInputAssemblerInputLayout;
  }
};
struct SSRPostProcessing : public RootSignatureMask {
  RootDescriptor<RootDescriptorType::ConstantBuffer> CameraBuffer;
  // RootDescriptor<RootDescriptorType::ConstantBuffer> CameraBuffer;
  RootDescriptorTable<1> InpColor;
  RootDescriptorTable<1> NormalBuffer;
  RootDescriptorTable<1> DepthBuffer;
  // RootDescriptorTable<1> DepthBuffer;
  RootDescriptorTable<1> OutputTexture;
  StaticSampler Sampler;

  SSRPostProcessing(const RootSignatureContext &context)
      : RootSignatureMask(context), CameraBuffer(this, {0}),
        // CameraBuffer(this, {1}),
        InpColor(this, {DescriptorRangeType::ShaderResource, {0}}),
        NormalBuffer(this, {DescriptorRangeType::ShaderResource, {1}}),
        DepthBuffer(this, {DescriptorRangeType::ShaderResource, {9}}),
        // DepthBuffer(this, {DescriptorRangeType::ShaderResource, {1}}),
        OutputTexture(this, {DescriptorRangeType::UnorderedAccess, {0}}),
        Sampler(this, {0}, Filter::Linear, TextureAddressMode::Clamp) {
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
  ShadowMapping::Textures ShadowMapTextures;
  DeferredShading::GBuffer GBuffer;
  MutableTexture PostProcessingBuffer;

  void MakeCompatible(const RenderTargetView &finalTarget,
                      ResourceAllocationContext &allocationContext) override;

  void Clear(CommandAllocator &allocator) override;

  explicit FrameResources(const ResourceAllocationContext &context);
};
