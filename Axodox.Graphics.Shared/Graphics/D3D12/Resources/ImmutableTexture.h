#pragma once
#include "Texture.h"
#include "ResourceAllocationContext.h"

namespace Axodox::Graphics::D3D12 {
class AXODOX_GRAPHICS_API ImmutableTexture {
public:
  ImmutableTexture(const ResourceAllocationContext &context,
                   const std::filesystem::path &path);
  ImmutableTexture(const ResourceAllocationContext &context,
                   const TextureData &textureData);

  operator GpuVirtualAddress() const;

private:
  TextureRef _texture;
  ShaderResourceViewRef _view;
  Infrastructure::event_subscription _allocatedSubscription;
};
} // namespace Axodox::Graphics::D3D12