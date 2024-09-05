#include "pch.h"
#include "GraphicsPipeline.h"
#include "Camera.h"

void FrameResources::MakeCompatible(
    const RenderTargetView &finalTarget,
    ResourceAllocationContext &allocationContext) {
  // Ensure depth buffer matches frame size
  if (!DepthBuffer ||
      !TextureDefinition::AreSizeCompatible(*DepthBuffer.Definition(),
                                            finalTarget.Definition())) {

    auto depthDefinition = finalTarget.Definition().MakeSizeCompatible(
        Format::D32_Float, TextureFlags::ShaderResourceDepthStencil);

    auto depthViews = TextureViewDefinitions::GetDepthStencilWithShaderView(
        Format::D32_Float, Format::R32_Float);

    DepthBuffer.Allocate(depthDefinition, depthViews);
  }

  // Ensure screen shader resource view
  if (!ScreenResourceView ||
      ScreenResourceView->Resource() != finalTarget.Resource()) {
    Texture screenTexture{finalTarget.Resource()};
    ScreenResourceView =
        allocationContext.CommonDescriptorHeap->CreateShaderResourceView(
            &screenTexture);
  }

  // Ensure post processing buffer
  if (!PostProcessingBuffer ||
      !TextureDefinition::AreSizeCompatible(*PostProcessingBuffer.Definition(),
                                            finalTarget.Definition())) {
    auto postProcessingDefinition = finalTarget.Definition().MakeSizeCompatible(
        Format::B8G8R8A8_UNorm, TextureFlags::UnorderedAccess);
    PostProcessingBuffer.Allocate(postProcessingDefinition);
  }

  // Ensure GBuffer compatibility
  GBuffer.MakeCompatible(finalTarget, allocationContext);
  ShadowMapTextures.MakeCompatible(finalTarget, allocationContext);
}

void FrameResources::Clear(CommandAllocator &allocator) {

  // Clears frame with background color
  DepthBuffer.DepthStencil()->Clear(allocator);
  GBuffer.Clear(allocator);
  ShadowMapTextures.Clear(allocator);
}

FrameResources::FrameResources(const ResourceAllocationContext &context)
    : Allocator(*context.Device), Fence(*context.Device),
      DynamicBuffer(*context.Device), DepthBuffer(context),
      PostProcessingBuffer(context), GBuffer(context),
      ShadowMapTextures(context) {}

ShadowMapping::Textures::Textures(const ResourceAllocationContext &context,
                                  const u32 N)

    : DepthBuffer(context,
                  TextureDefinition(Format::D32_Float, N, N, LODCOUNT,
                                    TextureFlags::ShaderResourceDepthStencil),
                  TextureViewDefinitions::GetDepthStencilWithShaderView(
                      Format::D32_Float, Format::R32_Float)) {}

void ShadowMapping::Textures::Clear(CommandAllocator &allocator) {
  DepthBuffer.DepthStencil()->Clear(allocator);
}

void ShadowMapping::Textures::TranslateToTarget(CommandAllocator &allocator) {
  allocator.TransitionResources({

      {DepthBuffer.operator Axodox::Graphics::D3D12::ResourceArgument(),
       ResourceStates::PixelShaderResource, ResourceStates::DepthWrite},
  });
}

void ShadowMapping::Textures::TranslateToView(CommandAllocator &allocator,
                                              const ResourceStates &newState) {
  allocator.TransitionResources({
      {DepthBuffer.operator Axodox::Graphics::D3D12::ResourceArgument(),
       ResourceStates::DepthWrite, newState},
  });
}

void DeferredShading::BindGBuffer(const GBuffer &buffers) {
  albedo = *buffers.Albedo.ShaderResource();
  normal = *buffers.Normal.ShaderResource();
  position = *buffers.Position.ShaderResource();
  materialValues = *buffers.MaterialValues.ShaderResource();
}

std::array<std::pair<MutableTexture *, Format>,
           DeferredShading::GBuffer::NumberOfBuffers>
DeferredShading::GBuffer::GetBuffersAndFormats() {
  std::array<Format, NumberOfBuffers> formats = GetGBufferFormats();
  std::array<MutableTexture *, NumberOfBuffers> buffers = GetBuffers();
  std::array<std::pair<MutableTexture *, Format>, NumberOfBuffers> zippedArray;

  std::transform(
      formats.begin(), formats.end(), buffers.begin(), zippedArray.begin(),
      [](Format f, MutableTexture *b) { return std::make_pair(b, f); });
  return zippedArray;
}

void DeferredShading::GBuffer::MakeCompatible(
    const RenderTargetView &finalTarget,
    ResourceAllocationContext &allocationContext) {
  auto UpdateTexture = [&finalTarget](MutableTexture &texture,
                                      const Format &format,
                                      const TextureFlags &flags) {
    if (!texture || !TextureDefinition::AreSizeCompatible(
                        *texture.Definition(), finalTarget.Definition())) {
      texture.Reset();
      auto definition =
          finalTarget.Definition().MakeSizeCompatible(format, flags);
      texture.Allocate(definition);
    }
  };
  for (auto &it : GetBuffersAndFormats()) {
    UpdateTexture(*it.first, it.second, TextureFlags::RenderTarget);
  }
}

void DeferredShading::GBuffer::Clear(CommandAllocator &allocator) {
  Albedo.RenderTarget()->Clear(allocator);
  Normal.RenderTarget()->Clear(allocator);
  Position.RenderTarget()->Clear(allocator);
  MaterialValues.RenderTarget()->Clear(allocator);
}

void DeferredShading::GBuffer::TranslateToTarget(CommandAllocator &allocator) {
  allocator.TransitionResources({

      {Albedo.operator Axodox::Graphics::D3D12::ResourceArgument(),
       ResourceStates::PixelShaderResource, ResourceStates::RenderTarget},
      {Normal.operator Axodox::Graphics::D3D12::ResourceArgument(),
       ResourceStates::PixelShaderResource, ResourceStates::RenderTarget},
      {Position.operator Axodox::Graphics::D3D12::ResourceArgument(),
       ResourceStates::PixelShaderResource, ResourceStates::RenderTarget},
      {MaterialValues.operator Axodox::Graphics::D3D12::ResourceArgument(),
       ResourceStates::PixelShaderResource, ResourceStates::RenderTarget},
  });
}

void DeferredShading::GBuffer::TranslateToView(CommandAllocator &allocator) {
  allocator.TransitionResources({
      {Albedo.operator Axodox::Graphics::D3D12::ResourceArgument(),
       ResourceStates::RenderTarget, ResourceStates::PixelShaderResource},
      {Normal.operator Axodox::Graphics::D3D12::ResourceArgument(),
       ResourceStates::RenderTarget, ResourceStates::PixelShaderResource},
      {Position.operator Axodox::Graphics::D3D12::ResourceArgument(),
       ResourceStates::RenderTarget, ResourceStates::PixelShaderResource},
      {MaterialValues.operator Axodox::Graphics::D3D12::ResourceArgument(),
       ResourceStates::RenderTarget, ResourceStates::PixelShaderResource},
  });
}

std::array<XMVECTOR, 8> GetFrustumCorners(const XMMATRIX invViewProj,
                                          const Camera &cam,
                                          const f32 nearPlane,
                                          const f32 farPlane) {

  XMMATRIX invProj = XMMatrixInverse(
      nullptr, XMMatrixPerspectiveFovRH(cam.GetAngle(), cam.GetAspect(),
                                        nearPlane, farPlane));

  XMMATRIX inv = XMMatrixMultiply(invProj, invViewProj);

  auto xTransf = ViewFrustumCoordinates::xRange;
  auto yTransf = ViewFrustumCoordinates::yRange;
  auto zTransf = ViewFrustumCoordinates::zRange;
  xTransf.second += xTransf.first;
  yTransf.second += yTransf.first;
  zTransf.second += zTransf.first;

  std::array<XMVECTOR, 8> frustumCorners;
  for (unsigned int x = 0; x < 2; ++x) {
    for (unsigned int y = 0; y < 2; ++y) {
      for (unsigned int z = 0; z < 2; ++z) {
        const u32 ind = 4 * x + 2 * y + z;
        const auto vec = XMVector4Transform(
            XMVECTOR{x * xTransf.second + xTransf.first,
                     y * yTransf.second + yTransf.first,
                     z * zTransf.second + zTransf.first, 1.0f},
            inv);
        frustumCorners[ind] = XMVectorDivide(vec, XMVectorSplatW(vec));
      }
    }
  }

  return frustumCorners;
}

template <const size_t N>
XMVECTOR GetCenter(const std::array<XMVECTOR, N> &inp) {
  auto sum = XMVECTOR{0, 0, 0, 0};
  for (const auto &v : inp) {
    sum = XMVectorAdd(sum, v);
  }
  return XMVectorDivide(sum, XMVectorReplicate(N));
}
void ShadowMapping::Buffer::Update(const Camera &cam, const LightData &light) {
  f32 closePlane = cam.GetZNear();
  const XMVECTOR lightP = XMLoadFloat4(&light.lightPos);
  for (auto &lod : lods) {

    std::array<XMVECTOR, 8> frustumCorners =
        GetFrustumCorners(cam.GetViewMatrix(), cam, closePlane, lod.farPlane);

    XMVECTOR center = GetCenter(frustumCorners);
    XMVECTOR lightPos;
    XMMATRIX lightView;
    XMMATRIX lightProj;
    if (light.lightPos.w < 0.01) {
      // directional
      lightPos = center + lightP;
      lightView = XMMatrixLookAtRH(lightPos, center, cam.GetWorldUp());
      float minX = std::numeric_limits<float>::max();
      float maxX = std::numeric_limits<float>::lowest();
      float minY = std::numeric_limits<float>::max();
      float maxY = std::numeric_limits<float>::lowest();
      float minZ = std::numeric_limits<float>::max();
      float maxZ = std::numeric_limits<float>::lowest();
      XMFLOAT4 trf;
      for (const auto &v : frustumCorners) {
        const auto trfV = XMVector4Transform(v, lightView);
        XMStoreFloat4(&trf, XMVectorDivide(trfV, XMVectorSplatW(trfV)));

        minX = std::min(minX, trf.x);
        maxX = std::max(maxX, trf.x);
        minY = std::min(minY, trf.y);
        maxY = std::max(maxY, trf.y);
        minZ = std::min(minZ, trf.z);
        maxZ = std::max(maxZ, trf.z);
      }

      if (minZ < 0) {
        minZ *= zMult;
      } else {
        minZ /= zMult;
      }
      if (maxZ < 0) {
        maxZ /= zMult;
      } else {
        maxZ *= zMult;
      }
      lightProj =
          XMMatrixOrthographicOffCenterRH(minX, maxX, minY, maxY, minZ, maxZ);

    } else {
      lightPos = lightP;
      assert("IMPLEMENT THIS");
    }

    lod.viewProj = XMMatrixMultiply(lightView, lightProj);
    closePlane = lod.farPlane;
  }
}

ShadowMapping::Buffer::Buffer(const Camera &cam, f32 closestFrustumEnd,
                              f32 midFrustumEnd) {

  lods[0].farPlane = closestFrustumEnd;
  lods[1].farPlane = midFrustumEnd;
  lods[2].farPlane = cam.GetZFar();
}
