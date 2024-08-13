
#pragma once
#include "pch.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Storage;
using namespace DirectX::PackedVector;
struct SimpleGraphicsRootDescription : public RootSignatureMask {
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

  // Debug constants
  struct PixelConstants {
    XMFLOAT4 mult;
    XMUINT4 swizzleorder;
    int useTexture;
  };
  struct DomainConstants {
    int useDisplacement;
  };

  RootDescriptor<RootDescriptorType::ConstantBuffer> vertexBuffer;
  RootDescriptor<RootDescriptorType::ConstantBuffer> hullBuffer;
  RootDescriptor<RootDescriptorType::ConstantBuffer> domainBuffer;
  RootDescriptor<RootDescriptorType::ConstantBuffer> pixelBuffer;
  RootDescriptor<RootDescriptorType::ConstantBuffer> cameraBuffer;
  RootDescriptor<RootDescriptorType::ConstantBuffer> modelBuffer;
  RootDescriptorTable<1> texture;

  RootDescriptorTable<1> heightMapForDomain;
  RootDescriptorTable<1> gradientsForPixel;

  explicit SimpleGraphicsRootDescription(const RootSignatureContext &context)
      : RootSignatureMask(context),
        vertexBuffer(this, {2}, ShaderVisibility::Vertex),
        hullBuffer(this, {1}, ShaderVisibility::Hull),
        domainBuffer(this, {1}, ShaderVisibility::Domain),
        pixelBuffer(this, {1}, ShaderVisibility::Pixel),
        cameraBuffer(this, {0}, ShaderVisibility::All),
        modelBuffer(this, {1}, ShaderVisibility::Vertex),
        texture(this, {DescriptorRangeType::ShaderResource},
                ShaderVisibility::Pixel),
        heightMapForDomain(this, {DescriptorRangeType::ShaderResource, {0}},
                           ShaderVisibility::Domain),
        gradientsForPixel(this, {DescriptorRangeType::ShaderResource, {1}},
                          ShaderVisibility::Pixel),
        _textureSampler(this, {0}, Filter::Linear, TextureAddressMode::Wrap,
                        ShaderVisibility::Pixel),
        _heightmapSamplerForDomain(this, {0}, Filter::Linear,
                                   TextureAddressMode::Wrap,
                                   ShaderVisibility::Domain) {
    Flags = RootSignatureFlags::AllowInputAssemblerInputLayout;
  }

private:
  StaticSampler _textureSampler;
  StaticSampler _heightmapSamplerForDomain;
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
