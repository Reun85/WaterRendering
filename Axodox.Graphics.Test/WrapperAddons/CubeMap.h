#pragma once
#include "../pch.h"
using namespace Axodox::Graphics::D3D12;
using namespace Axodox::Infrastructure;
using namespace Axodox::Storage;

struct CubeMapPaths {
  union {
    struct {
      std::filesystem::path PosX;
      std::filesystem::path NegX;
      std::filesystem::path PosY;
      std::filesystem::path NegY;
      std::filesystem::path PosZ;
      std::filesystem::path NegZ;
    };
    std::array<const std::filesystem::path, 6> paths;
  };
  ~CubeMapPaths() noexcept {};
};

/// <summary>
///  Represents a cubemap texture.
/// Supports parsing a equirectangular HDR image to a cubemap texture. BE WARNED, IT IS SLOW. 
/// You should use a pre-processed cubemap textures instead. but for testing purposes, it works great.
/// </summary>
class CubeMapTexture {
public:
  CubeMapTexture(const ResourceAllocationContext &context,
                 const CubeMapPaths &paths);
  CubeMapTexture(const ResourceAllocationContext &context,
                 const std::filesystem::path &hdrImagePath,
                 const std::optional<const u32> &size = std::nullopt);

  operator GpuVirtualAddress() const;

private:
  TextureRef _texture;
  ShaderResourceViewRef _view;
  Axodox::Infrastructure::event_subscription _allocatedSubscription;
};
