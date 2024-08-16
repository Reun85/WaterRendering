
#pragma once
#include "pch.h"

using namespace Axodox::Infrastructure;
using namespace Axodox::Storage;
using namespace DirectX::PackedVector;
struct SkyboxRootDescription : public RootSignatureMask {
  RootDescriptor<RootDescriptorType::ConstantBuffer> cameraBuffer;
  RootDescriptorTable<1> skybox;

  StaticSampler _textureSampler;

  explicit SkyboxRootDescription(const RootSignatureContext &context)
      : RootSignatureMask(context),
        cameraBuffer(this, {0}, ShaderVisibility::Vertex),

        skybox(this, {DescriptorRangeType::ShaderResource, {0}},
               ShaderVisibility::Pixel),
        _textureSampler(this, {0}, Filter::Linear, TextureAddressMode::Wrap,
                        ShaderVisibility::Pixel) {
    Flags = RootSignatureFlags::AllowInputAssemblerInputLayout;
  }
};
