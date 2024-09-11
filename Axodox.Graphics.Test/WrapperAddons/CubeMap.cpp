#include "pch.h"
#include "../pch.h"
#include "CubeMap.h"
#include <DirectXTex.h>
#include <ranges>
#include <algorithm>

#include <numbers>
using namespace std;
using namespace Axodox::Graphics::D3D12;
using namespace Axodox::Infrastructure;
using namespace Axodox::Storage;
using namespace winrt;

using namespace Windows::Foundation::Numerics;

enum class CubeMapFace : u8 {
  PosZ = 0,
  NegZ = 1,
  PosY = 2,
  NegY = 3,
  PosX = 4,
  NegX = 5
};

std::tuple<f32, f32, f32>
FaceCoordinatesToWorldCoordinates(u32 i, u32 j, u32 width, CubeMapFace face) {
  f32 a = 2.0f * static_cast<f32>(i) / static_cast<f32>(width);
  f32 b = 2.0f * static_cast<f32>(j) / static_cast<f32>(width);

  switch (face) {
    using enum CubeMapFace;
  case NegX:
    return {1.0, 1.0 - a, 1.0 - b};
    break;
  case NegZ:
    return {1.0 - a, -1.0, 1.0 - b};
    break;
  case PosX:
    return {-1.0, a - 1.0, 1.0 - b};
    break;
  case PosZ:
    return {a - 1.0, 1.0, 1.0 - b};
    break;
  case PosY:
    return {1.0 - b, a - 1.0, 1.0};
    break;
  case NegY:
    return {b - 1.0, a - 1.0, -1.0};
    break;
  }
}

// Lanczos kernel function
constexpr inline f32 LanczosKernel(f32 x, f32 a = 3.0f) {
  if (x == 0.0f)
    return 1.0f;
  if (x < -a || x > a)
    return 0.0f;
  return (a * sin(std::numbers::pi * x) * sin(std::numbers::pi * x / a)) /
         (std::numbers::pi * std::numbers::pi * x * x);
}

template <typename PixelType>
constexpr PixelType
LanczosInterpolation(f32 uf, f32 vf, u32 inSizex, u32 inSizey,
                     const std::span<const PixelType> &src, f32 a = 3.0f) {
  f32 sum = 0.0f;
  PixelType result = {};

  // Define the region of interest based on the Lanczos window size
  u32 uStart = static_cast<u32>(floor(uf - a + 1));
  u32 uEnd = static_cast<u32>(floor(uf + a));
  u32 vStart = static_cast<u32>(floor(vf - a + 1));
  u32 vEnd = static_cast<u32>(floor(vf + a));
  auto inPix = [&inSizex, &inSizey, &src](u32 u, u32 v) {
    u32 indx = u % inSizex;
    u32 indy = clamp(v, 0u, inSizey - 1u);
    u32 index = (indy * inSizex + indx);
    return src[index];
  };

  for (u32 u = uStart; u <= uEnd; ++u) {
    for (u32 v = vStart; v <= vEnd; ++v) {
      f32 lu = LanczosKernel(uf - u, a);
      f32 lv = LanczosKernel(vf - v, a);
      f32 weight = lu * lv;

      result += inPix(u, v) * weight;
      sum += weight;
    }
  }

  // Normalize the result to account for the total weight sum
  if (sum > 0.0f) {
    result /= sum;
  }

  return result;
}

template <typename PixelType>
constexpr PixelType
BiliniearInterpolation(f32 uf, f32 vf, u32 inSizex, u32 inSizey,
                       const std::span<const PixelType> &src) {
  // Use bilinear interpolation between the four surrounding pixels
  u32 ui = static_cast<u32>(floor(uf));
  // coord of pixel to bottom left
  u32 vi = static_cast<u32>(floor(vf));
  u32 u2 = ui + 1;
  // coords of pixel to top right
  u32 v2 = vi + 1;
  f32 mu = uf - static_cast<f32>(ui);
  // fraction of way across pixel
  f32 nu = vf - static_cast<f32>(vi);
  auto inPix = [&inSizex, &inSizey, &src](u32 u, u32 v) {
    u32 indx = u % inSizex;
    u32 indy = clamp(v, 0u, inSizey - 1u);
    u32 index = (indy * inSizex + indx);
    return src[index];
  };

  // Pixel values of four corners
  PixelType A = inPix(ui, vi);
  PixelType B = inPix(u2, vi);
  PixelType C = inPix(ui, v2);
  PixelType D = inPix(u2, v2);

  PixelType ret = A * (1 - mu) * (1 - nu) + B * mu * (1 - nu) +
                  C * (1 - mu) * nu + D * mu * nu;
  return ret;
}

template <typename PixelType>
constexpr void
ConvertEquirectangularToCubeMap(const std::span<const PixelType> &src,
                                const std::span<PixelType> &dest,
                                const u32 outSize, const u32 inSizex,
                                const u32 inSizey, const CubeMapFace &face) {
  for (u32 i = 0; i < outSize; ++i) {
    for (u32 j = 0; j < outSize; ++j) {
      auto [x, y, z] = FaceCoordinatesToWorldCoordinates(i, j, outSize, face);
      f32 theta = atan2(y, x); // [-pi, pi]
      f32 r = hypot(x, y);
      f32 phi = atan2(z, r); // [-pi/2, pi/2]

      // source imgcoords
      f32 uf = (theta + std::numbers::pi_v<f32>)*0.5f *
               std::numbers::inv_pi_v<f32> * inSizex;
      f32 vf = (std::numbers::pi_v<f32> / 2.f - phi) * 0.5f *
               std::numbers::inv_pi_v<f32> * inSizex;

      // PixelType ret = LanczosInterpolation(uf, vf, inSizex, inSizey, src);
      PixelType ret = BiliniearInterpolation(uf, vf, inSizex, inSizey, src);

      dest[j * outSize + i] = ret;
    }
  }
}

CubeMapTexture::CubeMapTexture(const ResourceAllocationContext &context,
                               const std::filesystem::path &hdrImagePath,
                               const std::optional<const u32> &size) {
  // Initialize the DirectXTex scratch image that will hold the HDR data
  DirectX::ScratchImage LoadedImage;

  // Load the .hdr file
  if (hdrImagePath.extension() == ".hdr") {
    // Load the .hdr file
    // (Note: This function is not thread-safe, so we need to make sure that
    // only one thread is calling it at a time.)
    HRESULT hr = DirectX::LoadFromHDRFile(hdrImagePath.native().c_str(),
                                          nullptr, LoadedImage);
    winrt::check_hresult(hr);
  } else {
    // Load the .hdr file
    // (Note: This function is not thread-safe, so we need to make sure that
    // only one thread is calling it at a time.)
    HRESULT hr =
        DirectX::LoadFromWICFile(hdrImagePath.native().c_str(),
                                 DirectX::WIC_FLAGS_NONE, nullptr, LoadedImage);

    winrt::check_hresult(hr);
  }
  // Get the image data from the ScratchImage

  // Check if the format is already R32G32B32A32_FLOAT
  if (LoadedImage.GetMetadata().format != DXGI_FORMAT_R32G32B32A32_FLOAT) {
    const DirectX::Image *image = LoadedImage.GetImage(0, 0, 0);
    assert(image);
    DirectX::ScratchImage convertedImage;

    // Convert the image to R32G32B32A32_FLOAT format
    HRESULT hr = DirectX::Convert(
        *image,                         // The image to convert
        DXGI_FORMAT_R32G32B32A32_FLOAT, // Target format
        DirectX::TEX_FILTER_DEFAULT,    // Filtering option
        DirectX::TEX_THRESHOLD_DEFAULT, // Threshold for color keying (not used
                                        // here)
        convertedImage // Output ScratchImage with converted data
    );

    winrt::check_hresult(hr);

    // Replace the original ScratchImage with the converted one
    LoadedImage = std::move(convertedImage);
  }
  const DirectX::Image *image = LoadedImage.GetImage(0, 0, 0);
  assert(image);

  const u8 faceCount = 6;

  const u32 width = size.value_or(image->width / 4);
  const u32 height = size.value_or(width);
  TextureData textureData(Format(image->format), width, height, faceCount);
  using PixelType = float4;
  const std::span<const PixelType> src(
      reinterpret_cast<const PixelType *>(image->pixels),
      image->width * image->height);
  for (int i = 0; i < faceCount; ++i) {
    std::span<PixelType> dest = textureData.AsTypedSpan<PixelType>(nullptr, i);
    ConvertEquirectangularToCubeMap<float4>(src, dest, width, image->width,
                                            image->height, (CubeMapFace)i);
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

CubeMapTexture::CubeMapTexture(const ResourceAllocationContext &context,
                               const CubeMapPaths &inp) {
  const u8 faceCount = 6;

  std::vector<TextureData> data;
  data.reserve(faceCount);
  for (const auto &p : inp.paths) {
    data.push_back(TextureData::FromFile(p));
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