#include "pch.h"
#include "CubeMap.h"

using namespace std;
using namespace Axodox::Graphics::D3D12;
using namespace Axodox::Infrastructure;
using namespace Axodox::Storage;

CubeMapTexture::CubeMapTexture(
    const ResourceAllocationContext &context,
    const std::array<const std::filesystem::path, 6> &paths) {

  const u8 faceCount = 6;

  std::vector<TextureData> data;
  data.reserve(faceCount);
  for (const auto &p : paths) {
    data.push_back(TextureData::FromFile(app_folder() / p));
  }

  const TextureHeader &header = data[0].Header();
  for (int i = 0; i < faceCount; i++) {
    assert(header.Width == data[i].Header().Width &&
               header.Height == data[i].Header().Height &&
               header.PixelFormat == data[i].Header().PixelFormat,
           "Skybox textures must have the same size and format");
    assert(data[i].Header().Depth == 0, "Skybox textures must be 2D");
    assert(data[i].Header().ArraySize == 0, "Skybox textures must be 2D");
  }

  TextureData textureData(header.PixelFormat, header.Width, header.Height,
                          faceCount);
  for (int i = 0; i < faceCount; ++i) {

    std::span<const u8> srcPtr = data[i].AsRawSpan();
    std::span<u8> destPtr = textureData.AsRawSpan(nullptr, i);

    assert(destPtr.size_bytes() == srcPtr.size_bytes(),
           "Skybox face size mismatch");
    std::memcpy(destPtr.data(), srcPtr.data(), srcPtr.size_bytes());
  }

  _texture = context.ResourceAllocator->CreateTexture(textureData.Definition());

  _allocatedSubscription = _texture->Allocated([this, context,
                                                data = move(textureData)](
                                                   Resource *resource) {
    context.ResourceUploader->EnqueueUploadTask(resource, &data);
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = static_cast<DXGI_FORMAT>(data.Definition().PixelFormat);
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MostDetailedMip = 0;
    srvDesc.TextureCube.MipLevels = -1;
    srvDesc.TextureCube.ResourceMinLODClamp = 0;
    srvDesc.TextureCube.MostDetailedMip = 0;
    srvDesc.TextureCube.MipLevels = (UINT)-1;
    srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
    _view = context.CommonDescriptorHeap->CreateDescriptor<ShaderResourceView>(
        resource->get(), &srvDesc);
  });
}

CubeMapTexture::operator GpuVirtualAddress() const { return *_view; }
