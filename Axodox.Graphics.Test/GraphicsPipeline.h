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

struct WaterGraphicRootDescription : public RootSignatureMask {
  struct ModelConstants {
    XMFLOAT4X4 mMatrix;
  };

  struct VertexConstants {
    struct InstanceData {
      XMFLOAT2 scaling;
      XMFLOAT2 offset;
    };
    std::array<InstanceData, DefaultsValues::App::maxInstances> instanceData;
  };
  struct HullConstants {
    struct InstanceData {
      // zneg,xneg, zpos, xpos
      union {
        struct {
          f32 zneg;
          f32 xneg;
          f32 zpos;
          f32 xpos;
        };
        XMFLOAT4 TesselationFactor;
      };
    };
    std::array<InstanceData, DefaultsValues::App::maxInstances> instanceData;
  };
  struct OceanData {
    VertexConstants vertexConstants;
    HullConstants hullConstants;
    u16 N = 0;
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
      PixelShaderPBRData data = {};
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
    float Roughness = 0.150f;
    float3 _TipColor = float3(231.f, 231.f, 231.f) / 255.f;
    float foamDepthFalloff = 0.245f;
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

  static std::vector<WaterGraphicRootDescription::OceanData> &
  CollectOceanQuadInfoWithQuadTree(
      std::vector<WaterGraphicRootDescription::OceanData> &vec,
      const Camera &cam, const XMMATRIX &mMatrix,
      const float &quadTreeDistanceThreshold,
      const std::optional<RuntimeResults *> &runRes = std::nullopt);
};

struct DeferredShading : public RootSignatureMask {
  struct GBuffer : ShaderBuffers {
    MutableTexture Albedo;
    MutableTexture Normal;
    MutableTexture MaterialValues;

    const static constexpr u8 NumberOfBuffers = 3;

    std::array<const RenderTargetView *, NumberOfBuffers>
    GetGBufferViews() const {
      return {Albedo.RenderTarget(), Normal.RenderTarget(),
              MaterialValues.RenderTarget()};
    }

    //
    static const std::array<Format, NumberOfBuffers> &GetGBufferFormats() {
      using enum Axodox::Graphics::D3D12::Format;
      // Returns a static reference, so that we can turn this into a initializer
      // list easily
      const static std::array<Format, NumberOfBuffers> formats = {
          {B8G8R8A8_UNorm, R16G16B16A16_Float, R16G16B16A16_Float}};
      return formats;
    };

    std::array<MutableTexture *, NumberOfBuffers> GetBuffers() {
      return {&Albedo, &Normal, &MaterialValues};
    }
    std::array<std::pair<MutableTexture *, Format>, NumberOfBuffers>
    GetBuffersAndFormats();

    explicit GBuffer(const ResourceAllocationContext &context)
        : Albedo(context), Normal(context), MaterialValues(context) {}

    void MakeCompatible(const RenderTargetView &finalTarget,
                        ResourceAllocationContext &allocationContext) override;

    void Clear(CommandAllocator &allocator) override;
    void TranslateToTarget(CommandAllocator &allocator);
    void TranslateToView(CommandAllocator &allocator);
    ~GBuffer() override = default;
  };

  struct DeferredShaderBuffers {
    float3 _TipColor = float3(173.f, 173.f, 173.f) / 255.f;
    float EnvMapMult = 1.f;
  };

  RootDescriptorTable<1> albedo;
  RootDescriptorTable<1> normal;
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
        materialValues(this, {DescriptorRangeType::ShaderResource, 3}),
        geometryDepth(this, {DescriptorRangeType::ShaderResource, 4},
                      ShaderVisibility::Pixel),
        skybox(this, {DescriptorRangeType::ShaderResource, {5}},
               ShaderVisibility::Pixel),
        gradientsHighest(this, {DescriptorRangeType::ShaderResource, 20},
                         ShaderVisibility::Pixel),
        gradientsMedium(this, {DescriptorRangeType::ShaderResource, 21},
                        ShaderVisibility::Pixel),
        gradientsLowest(this, {DescriptorRangeType::ShaderResource, 22},
                        ShaderVisibility::Pixel),
        lightingBuffer(this, {1}, ShaderVisibility::Pixel),
        cameraBuffer(this, {31}, ShaderVisibility::Pixel),
        debugBuffer(this, {9}, ShaderVisibility::Pixel),
        deferredShaderBuffer(this, {2}, ShaderVisibility::Pixel),
        Sampler(this, {0}, Filter::Linear, TextureAddressMode::Clamp) {
    Flags = RootSignatureFlags::AllowInputAssemblerInputLayout;
  }

  void BindGBuffer(const GBuffer &buffers);
};

struct ShadowMapping : public RootSignatureMask {
  constexpr static size_t LODCOUNT = 3;

  struct Textures : ShaderBuffers {
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
    ~Textures() override = default;
  };

  struct Data {
    struct LOD {
      XMMATRIX lightViewProjFromCamViewProj;
      f32 farPlane;
    };

    std::array<LOD, LODCOUNT> lods;

    constexpr static f32 zMult = 10.f;

    GpuVirtualAddress Upload(DynamicBufferManager &manager) const;
    void Update(const Camera &cam, const LightData &light);
    explicit Data(const Camera &cam, f32 closestFrustumEnd = 200.f,
                  f32 midFrustumEnd = 600.f);

  private:
    struct GPULayout {
      std::array<XMFLOAT4X4, DefaultsValues::App::maxShadowMapMatrices>
          lightSpaceMatrices;
      u32 shadowMapMatrixCount;
    };
  };

  RootDescriptor<RootDescriptorType::ConstantBuffer> vertexBuffer;
  RootDescriptor<RootDescriptorType::ConstantBuffer> hullBuffer;
  RootDescriptor<RootDescriptorType::ConstantBuffer> cameraBuffer;
  RootDescriptor<RootDescriptorType::ConstantBuffer> modelBuffer;
  RootDescriptor<RootDescriptorType::ConstantBuffer> debugBuffer;
  RootDescriptor<RootDescriptorType::ConstantBuffer> lightingMatrices;

  // Optional
  RootDescriptorTable<1> texture;

  RootDescriptorTable<1> heightMapHighest;
  RootDescriptorTable<1> heightMapMedium;
  RootDescriptorTable<1> heightMapLowest;
  RootDescriptorTable<1> gradientsHighest;
  RootDescriptorTable<1> gradientsMedium;
  RootDescriptorTable<1> gradientsLowest;

  StaticSampler _textureSampler;

  explicit ShadowMapping(const RootSignatureContext &context)
      : RootSignatureMask(context),
        vertexBuffer(this, {2}, ShaderVisibility::Vertex),
        hullBuffer(this, {1}, ShaderVisibility::Hull),
        cameraBuffer(this, {0}, ShaderVisibility::All),
        modelBuffer(this, {1}, ShaderVisibility::Vertex),
        debugBuffer(this, {9}, ShaderVisibility::All),
        lightingMatrices(this, {4}, ShaderVisibility::Geometry),

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

struct SSRPostProcessing : public RootSignatureMask {
  RootDescriptor<RootDescriptorType::ConstantBuffer> CameraBuffer;
  RootDescriptorTable<1> InpColor;
  RootDescriptorTable<1> NormalBuffer;
  RootDescriptorTable<1> DepthBuffer;
  RootDescriptorTable<1> OutputTexture;
  StaticSampler Sampler;

  explicit SSRPostProcessing(const RootSignatureContext &context)
      : RootSignatureMask(context), CameraBuffer(this, {0}),
        InpColor(this, {DescriptorRangeType::ShaderResource, {0}}),
        NormalBuffer(this, {DescriptorRangeType::ShaderResource, {1}}),
        DepthBuffer(this, {DescriptorRangeType::ShaderResource, {9}}),
        OutputTexture(this, {DescriptorRangeType::UnorderedAccess, {0}}),
        Sampler(this, {0}, Filter::Linear, TextureAddressMode::Clamp) {
    Flags = RootSignatureFlags::AllowInputAssemblerInputLayout;
  }
};

struct BasicShaderMask : RootSignatureMask {
  RootDescriptorTable<1> texture;
  RootDescriptor<RootDescriptorType::ConstantBuffer> camera;
  RootDescriptor<RootDescriptorType::ConstantBuffer> model;

  StaticSampler _textureSampler;

  struct ModelConstants {
    XMFLOAT4X4 mMatrix;
  };

  explicit BasicShaderMask(const RootSignatureContext &context)
      : RootSignatureMask(context),
        texture(this, {DescriptorRangeType::ShaderResource, {0}},
                ShaderVisibility::Pixel),
        camera(this, {0}), model(this, {1}),
        _textureSampler(this, {0}, Filter::Linear, TextureAddressMode::Wrap,
                        ShaderVisibility::All) {
    Flags = RootSignatureFlags::AllowInputAssemblerInputLayout;
  }
};

struct BasicShader : ShaderJob {
  struct Inp {
    const GpuVirtualAddress &camera;
    const GpuVirtualAddress &modelTransform;
    const std::optional<GpuVirtualAddress> texture;
    const ImmutableMesh &mesh;
  };

  using ShaderMask = BasicShaderMask;
  RootSignature<ShaderMask> Signature;
  PipelineState pipeline;

  BasicShader(PipelineStateProvider &pipelineProvider, GraphicsDevice &device,
              VertexShader *vs, PixelShader *ps);

  static BasicShader WithDefaultShaders(PipelineStateProvider &pipelineProvider,
                                        GraphicsDevice &device);
  void Pre(CommandAllocator &allocator) const override;
  void Run(CommandAllocator &allocator, DynamicBufferManager &buffermanager,
           const Inp &inp) const;
  ~BasicShader() override = default;
};

struct FrameResources : ShaderBuffers {
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

  ~FrameResources() override = default;
};
