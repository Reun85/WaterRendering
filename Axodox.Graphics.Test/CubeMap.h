#pragma once
#include "pch.h"
using namespace Axodox::Graphics::D3D12;
using namespace Axodox::Infrastructure;
using namespace Axodox::Storage;
class CubeMapTexture {

public:
  CubeMapTexture(const ResourceAllocationContext &context,
                 const std::array<const std::filesystem::path, 6> &paths);

  operator GpuVirtualAddress() const;

private:
  TextureRef _texture;
  ShaderResourceViewRef _view;
  Axodox::Infrastructure::event_subscription _allocatedSubscription;
};
