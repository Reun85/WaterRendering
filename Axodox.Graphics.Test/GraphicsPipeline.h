
#pragma once
#include "pch.h"

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
    };
    std::array<PixelLightData, ShaderConstantCompat::maxLightCount> lights;
    int lightCount;

    static constexpr PixelLighting SunData() {
      PixelLighting data;
      data.lightCount = 1;
      data.lights[0].lightPos = XMFLOAT4(-0.217, 0.393, -0.379, 0);
      data.lights[0].lightColor = XMFLOAT4(0.6, 0.4, 0.259, 3.787f);
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
      data.SurfaceColor = float3(0.109, 0.340, 0.589);
      data.Roughness = 0.285;
      data.SubsurfaceScattering = 0.571f;
      data.Sheen = 0.163f;
      data.SheenTint = 0.366f;
      data.Anisotropic = 0.0f;
      data.SpecularStrength = 0.552f;
      data.Metallic = 0.0f;
      data.SpecularTint = 0.0f;
      data.Clearcoat = 0.157f;
      data.ClearcoatGloss = 0.323;
      return data;
    }
  };

  RootDescriptor<RootDescriptorType::ConstantBuffer> vertexBuffer;
  RootDescriptor<RootDescriptorType::ConstantBuffer> hullBuffer;
  RootDescriptor<RootDescriptorType::ConstantBuffer> cameraBuffer;
  RootDescriptor<RootDescriptorType::ConstantBuffer> modelBuffer;
  RootDescriptor<RootDescriptorType::ConstantBuffer> debugBuffer;
  RootDescriptor<RootDescriptorType::ConstantBuffer> lightingBuffer;
  RootDescriptor<RootDescriptorType::ConstantBuffer> waterPBRBuffer;
  RootDescriptorTable<1> texture;

  RootDescriptorTable<1> skybox;
  RootDescriptorTable<1> heightMap1;
  RootDescriptorTable<1> heightMap2;
  RootDescriptorTable<1> heightMap3;
  RootDescriptorTable<1> gradients1;
  RootDescriptorTable<1> gradients2;
  RootDescriptorTable<1> gradients3;

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

        skybox(this, {DescriptorRangeType::ShaderResource, {1}},
               ShaderVisibility::Pixel),
        texture(this, {DescriptorRangeType::ShaderResource, {0}},
                ShaderVisibility::Pixel),
        heightMap1(this, {DescriptorRangeType::ShaderResource, {0}},
                   ShaderVisibility::Domain),
        heightMap2(this, {DescriptorRangeType::ShaderResource, {1}},
                   ShaderVisibility::Domain),
        heightMap3(this, {DescriptorRangeType::ShaderResource, {2}},
                   ShaderVisibility::Domain),
        gradients1(this, {DescriptorRangeType::ShaderResource, {3}},
                   ShaderVisibility::Domain),
        gradients2(this, {DescriptorRangeType::ShaderResource, {4}},
                   ShaderVisibility::Domain),
        gradients3(this, {DescriptorRangeType::ShaderResource, {5}},
                   ShaderVisibility::Domain),
        _textureSampler(this, {0}, Filter::Linear, TextureAddressMode::Wrap,
                        ShaderVisibility::All) {
    Flags = RootSignatureFlags::AllowInputAssemblerInputLayout;
  }
};

struct FrameResources {
  CommandAllocator Allocator;
  CommandFence Fence;
  CommandFenceMarker Marker;
  DynamicBufferManager DynamicBuffer;

  MutableTexture DepthBuffer;
  descriptor_ptr<ShaderResourceView> ScreenResourceView;

  explicit FrameResources(const ResourceAllocationContext &context)
      : Allocator(*context.Device), Fence(*context.Device),
        DynamicBuffer(*context.Device), DepthBuffer(context) {}
};
