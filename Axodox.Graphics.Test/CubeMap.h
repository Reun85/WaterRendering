#pragma once
#include "pch.h"
using namespace Axodox::Graphics::D3D12;
using namespace Axodox::Infrastructure;
using namespace Axodox::Storage;

struct CubeMapPaths {
public:
  struct SidesInfo {
    const std::filesystem::path PosX;
    const std::filesystem::path NegX;
    const std::filesystem::path PosY;
    const std::filesystem::path NegY;
    const std::filesystem::path PosZ;
    const std::filesystem::path NegZ;
  };
  union {
    SidesInfo sides;
    std::array<const std::filesystem::path, 6> paths;
  };
  ~CubeMapPaths() noexcept {};
};

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
