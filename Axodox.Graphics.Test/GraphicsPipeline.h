
#pragma once
#include "pch.h"
#include "Camera.h"
#include "QuadTree.h"
#include <fstream>
#include <string.h>
#include "Defaults.h"
#include "WaterMath.h"
#include "Helpers.h"
#include "pix3.h"

using namespace std;
using namespace winrt;

using namespace Windows;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::Foundation::Numerics;
using namespace Windows::UI;
using namespace Windows::UI::Core;
using namespace Windows::UI::Composition;

using namespace Axodox::Graphics::D3D12;
using namespace Axodox::Infrastructure;
using namespace Axodox::Storage;
using namespace DirectX;
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
    XMFLOAT2 scaling;
    XMFLOAT2 offset;
  };
  struct HullConstants {
    // zneg,xneg, zpos, xpos
    XMFLOAT4 TesselationFactor;
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
  RootDescriptorTable<1> gradientsForDomain;

private:
  StaticSampler _textureSampler;
  StaticSampler _heightmapSamplerForDomain;

public:
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
        gradientsForDomain(this, {DescriptorRangeType::ShaderResource, {1}},
                           ShaderVisibility::Domain),
        _textureSampler(this, {0}, Filter::Linear, TextureAddressMode::Wrap,
                        ShaderVisibility::Pixel),
        _heightmapSamplerForDomain(this, {0}, Filter::Linear,
                                   TextureAddressMode::Wrap,
                                   ShaderVisibility::Domain) {
    Flags = RootSignatureFlags::AllowInputAssemblerInputLayout;
  }
};
