#include "pch.h"
#include "GraphicsPipeline.h"
#include "QuadTree.h"
#include "Frustum.hpp"
namespace Reun::Graphics {

void FrameResources::Wait() {
  if (Marker)
    Fence.Await(Marker);
}
void FrameResources::MakeCompatible(
    const RenderTargetView &finalTarget,
    ResourceAllocationContext &allocationContext) {
  // Ensure depth buffer matches frame size
  if (!DepthBuffer ||
      !TextureDefinition::AreSizeCompatible(*DepthBuffer.Definition(),
                                            finalTarget.Definition())) {
    auto depthDefinition = finalTarget.Definition().MakeSizeCompatible(
        Format::R32_Typeless, TextureFlags::ShaderResourceDepthStencil);

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
  DepthBuffer.DepthStencil()->Clear(allocator, 1);
  GBuffer.Clear(allocator);
  ShadowMapTextures.Clear(allocator);
}

FrameResources::FrameResources(const ResourceAllocationContext &context)
    : Allocator(*context.Device), Fence(*context.Device),
      DynamicBuffer(*context.Device), DepthBuffer(context),
      ShadowMapTextures(context), GBuffer(context),
      PostProcessingBuffer(context) {}

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

ResourceTransitor<1>
ShadowMapping::Textures::TranslateToTarget(CommandAllocator &allocator) const {
  return ResourceTransitor<1>(
      allocator,
      {
          {DepthBuffer.operator Axodox::Graphics::D3D12::ResourceArgument(),
           ResourceStates::PixelShaderResource, ResourceStates::DepthWrite},
      });
}
ResourceTransitor<1>
ShadowMapping::Textures::TranslateToView(CommandAllocator &allocator,
                                         const ResourceStates &newState) const {
  return ResourceTransitor<1>(
      allocator,
      {
          {DepthBuffer.operator Axodox::Graphics::D3D12::ResourceArgument(),
           ResourceStates::DepthWrite, newState},
      });
}

void DeferredShading::BindGBuffer(const GBuffer &buffers) {
  albedo = *buffers.Albedo.ShaderResource();
  normal = *buffers.Normal.ShaderResource();
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
    const RenderTargetView &finalTarget, ResourceAllocationContext &) {
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
  MaterialValues.RenderTarget()->Clear(allocator);
}

ResourceTransitor<4>
DeferredShading::GBuffer::TranslateToTarget(CommandAllocator &allocator) const {
  return ResourceTransitor<4>(
      allocator,
      {
          {Albedo.operator Axodox::Graphics::D3D12::ResourceArgument(),
           ResourceStates::PixelShaderResource, ResourceStates::RenderTarget},
          {Normal.operator Axodox::Graphics::D3D12::ResourceArgument(),
           ResourceStates::PixelShaderResource, ResourceStates::RenderTarget},
          {MaterialValues.operator Axodox::Graphics::D3D12::ResourceArgument(),
           ResourceStates::PixelShaderResource, ResourceStates::RenderTarget},
      });
}

ResourceTransitor<4>
DeferredShading::GBuffer::TranslateToView(CommandAllocator &allocator) const {
  return ResourceTransitor<4>(
      allocator,
      {
          {Albedo.operator Axodox::Graphics::D3D12::ResourceArgument(),
           ResourceStates::RenderTarget, ResourceStates::PixelShaderResource},
          {Normal.operator Axodox::Graphics::D3D12::ResourceArgument(),
           ResourceStates::RenderTarget, ResourceStates::PixelShaderResource},
          {MaterialValues.operator Axodox::Graphics::D3D12::ResourceArgument(),
           ResourceStates::RenderTarget, ResourceStates::PixelShaderResource},
      });
}

std::array<XMVECTOR, 8> GetFrustumCorners(const XMMATRIX &invViewProj,
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
GpuVirtualAddress
ShadowMapping::Data::Upload(DynamicBufferManager &manager) const {
  GPULayout data = {};
  for (int i = 0; i < LODCOUNT; ++i) {
    XMStoreFloat4x4(&data.lightSpaceMatrices[i],
                    XMMatrixTranspose(lods[i].lightViewProjFromCamViewProj));
  }
  data.shadowMapMatrixCount = LODCOUNT;
  return manager.AddBuffer(data);
}
void ShadowMapping::Data::Update(const Camera &cam, const LightData &light) {
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
      lightView = XMMatrixIdentity();
      assert("IMPLEMENT THIS");
    }

    lod.lightViewProjFromCamViewProj = XMMatrixMultiply(lightView, lightProj);
    // Todo: multiply by inverse from front or back?
    lod.lightViewProjFromCamViewProj = XMMatrixMultiply(
        cam.GetINVViewProj(), lod.lightViewProjFromCamViewProj);

    closePlane = lod.farPlane;
  }
}

ShadowMapping::Data::Data(const Camera &cam, f32 closestFrustumEnd,
                          f32 midFrustumEnd) {
  lods[0].farPlane = closestFrustumEnd;
  lods[1].farPlane = midFrustumEnd;
  lods[2].farPlane = cam.GetZFar();
}

std::vector<WaterGraphicRootDescription::OceanData> &
WaterGraphicRootDescription::CollectOceanQuadInfoWithQuadTree(
    std::vector<WaterGraphicRootDescription::OceanData> &vec, const Camera &cam,
    const XMMATRIX &mMatrix, const float &quadTreeDistanceThreshold,
    const Depth &MaxDepth, const DebugValues &debugValues,
    const std::optional<RuntimeResults *> &runtimeResults) {
  float2 fullSizeXZ = {DefaultsValues::App::oceanSize,
                       DefaultsValues::App::oceanSize};

  float3 center = {0, 0, 0};
  QuadTree qt;
  XMFLOAT3 camUsedPos;
  XMVECTOR tmp = cam.GetEye();
  XMStoreFloat3(&camUsedPos, tmp);
  XMFLOAT3 camDir;
  tmp = cam.GetForward();
  XMStoreFloat3(&camDir, tmp);

  decltype(std::chrono::high_resolution_clock::now()) start;
  if (runtimeResults)
    start = std::chrono::high_resolution_clock::now();

  qt.Build(center, fullSizeXZ, float3(camUsedPos.x, camUsedPos.y, camUsedPos.z),
           float3(camDir.x, camDir.y, camDir.z),

           cam.GetFrustum(), mMatrix, quadTreeDistanceThreshold, MaxDepth);

  if (runtimeResults) {

    auto &x = **runtimeResults;
    x.QuadTreeBuildTime +=
        std::chrono::duration_cast<decltype(x.QuadTreeBuildTime)>(
            std::chrono::high_resolution_clock::now() - start);

    x.qtNodes = qt.GetSize();
    x.drawnNodes = 0;
  }
  // The best choice is to upload planeBottomLeft and
  // planeTopRight and kinda of UV coordinate that can go
  // outside [0,1] and the fract is the actual UV value.

  // Fill buffer with Quad Info
  {
    start = std::chrono::high_resolution_clock::now();
    auto *curr = &vec.emplace_back();

    for (auto it = qt.begin(); it != qt.end(); ++it) {
      if (runtimeResults) {
        auto &x = **runtimeResults;
        x.NavigatingTheQuadTree +=
            std::chrono::duration_cast<decltype(x.NavigatingTheQuadTree)>(
                (std::chrono::high_resolution_clock::now() - start));
        x.drawnNodes++;
      }

      static float div = 1.f;
      {
        curr->vertexConstants.instanceData[curr->N].scaling = {
            it->size.x / div, it->size.y / div};
        curr->vertexConstants.instanceData[curr->N].offset = {it->center.x,
                                                              it->center.y};
      }
      if (!debugValues.calculateParallax()) {
        start = std::chrono::high_resolution_clock::now();

        auto res = it.GetSmallerNeighbor();

        (*runtimeResults)->NavigatingTheQuadTree += std::chrono::duration_cast<
            decltype((*runtimeResults)->NavigatingTheQuadTree)>(
            (std::chrono::high_resolution_clock::now() - start));
        static const constexpr auto l = [](const float x) -> float {
          if (x == 0)
            return 1;
          else
            return x;
        };
        curr->hullConstants.instanceData[curr->N].AsXMFLOAT4() = {
            l(res.zneg), l(res.xneg), l(res.zpos), l(res.xpos)};
      }

      curr->N = curr->N + 1;
      if (curr->N == DefaultsValues::App::maxInstances) {
        curr = &vec.emplace_back();
      }

      start = std::chrono::high_resolution_clock::now();
    }

    // If a quarter of the capacity is unused shrink the vector in a
    // way that the unused capacity is halfed
    // how though?
  }
  return vec;
}

BasicShader::BasicShader(PipelineStateProvider &pipelineProvider,
                         GraphicsDevice &device, VertexShader *vs,
                         PixelShader *ps)
    : Signature(device),
      pipeline(
          pipelineProvider
              .CreatePipelineStateAsync(GraphicsPipelineStateDefinition{
                  .RootSignature = &Signature,
                  .VertexShader = vs,
                  .PixelShader = ps,
                  .RasterizerState = RasterizerFlags::CullClockwise,
                  .DepthStencilState = DepthStencilMode::WriteDepth,
                  .InputLayout = VertexPositionNormalTexture::Layout,
                  .RenderTargetFormats = std::initializer_list(
                      std::to_address(
                          DeferredShading::GBuffer::GetGBufferFormats()
                              .begin()),
                      std::to_address(
                          DeferredShading::GBuffer::GetGBufferFormats().end())),
                  .DepthStencilFormat = Format::D32_Float})
              .get()) {}

BasicShader
BasicShader::WithDefaultShaders(PipelineStateProvider &pipelineProvider,
                                GraphicsDevice &device) {
  VertexShader vs(app_folder() / L"BasicVS.cso");
  PixelShader ps(app_folder() / L"BasicPS.cso");

  return BasicShader(pipelineProvider, device, &vs, &ps);
}

void BasicShader::Pre(CommandAllocator &allocator) const {
  pipeline.Apply(allocator);
}

void BasicShader::Run(CommandAllocator &allocator, DynamicBufferManager &,
                      const Inp &inp) const {
  auto mask = Signature.Set(allocator, RootSignatureUsage::Graphics);
  mask.camera = inp.camera;
  mask.model = inp.modelTransform;
  if (inp.texture)
    mask.texture = *inp.texture;

  inp.mesh.Draw(allocator);
}

PostProcessingShader::PostProcessingShader(
    PipelineStateProvider &pipelineProvider, GraphicsDevice &device,
    ComputeShader *cs)
    : Signature(device),

      pipeline(pipelineProvider
                   .CreatePipelineStateAsync(ComputePipelineStateDefinition{
                       .RootSignature = &Signature, .ComputeShader = cs})
                   .get()) {}

PostProcessingShader PostProcessingShader::WithDefaultShaders(
    PipelineStateProvider &pipelineProvider, GraphicsDevice &device) {
  ComputeShader cs{app_folder() / L"SSRPostProcessingShader.cso"};

  return PostProcessingShader(pipelineProvider, device, &cs);
}

void PostProcessingShader::Pre(CommandAllocator &allocator) const {
  pipeline.Apply(allocator);
}

void PostProcessingShader::Run(CommandAllocator &allocator,
                               DynamicBufferManager &, const Inp &inp) const {
  auto mask = Signature.Set(allocator, RootSignatureUsage::Compute);
  mask.InpColor = inp.inp;
  mask.DepthBuffer = inp.depthBuffer;
  mask.NormalBuffer = inp.normalBuffer;
  mask.OutputTexture = inp.textureBuffer;
  mask.CameraBuffer = inp.camera;

  pipeline.Apply(allocator);

  allocator.Dispatch(inp.X, inp.Y);

  if (inp.output) {
    auto res = *inp.output;
    allocator.TransitionResources(
        {{inp.textureBuffer, ResourceStates::UnorderedAccess,
          ResourceStates::CopySource},
         {res, ResourceStates::NonPixelShaderResource,
          ResourceStates::CopyDest}});

    allocator.CopyResource(inp.textureBuffer, res);

    allocator.TransitionResources(
        {{inp.textureBuffer, ResourceStates::CopySource,
          ResourceStates::UnorderedAccess},
         {res, ResourceStates::CopyDest, ResourceStates::RenderTarget}});
  }
}

WaterRenderPipelines
WaterRenderPipelines::Create(GraphicsDevice &device,
                             PipelineStateProvider &pipelineStateProvider_,
                             CreateSettings settings) {

  RootSignature<WaterGraphicRootDescription> waterRootSignature{device};

  VertexShader simpleVertexShader{app_folder() / L"VertexShader.cso"};
  PixelShader simplePixelShader{app_folder() / L"PixelShader.cso"};
  HullShader hullShader{app_folder() / L"hullShader.cso"};
  DomainShader domainShader{app_folder() / L"domainShader.cso"};

  auto &gBufferFormats = DeferredShading::GBuffer::GetGBufferFormats();

  GraphicsPipelineStateDefinition waterPipelineStateDefinition{
      .RootSignature = &waterRootSignature,
      .VertexShader = &simpleVertexShader,
      .DomainShader = &domainShader,
      .HullShader = &hullShader,
      .PixelShader = &simplePixelShader,
      .RasterizerState = settings.rasterizerState,
      .DepthStencilState = DepthStencilMode::WriteDepth,
      .InputLayout = VertexPosition::Layout,
      .TopologyType = PrimitiveTopologyType::Patch,
      .RenderTargetFormats =
          std::initializer_list(std::to_address(gBufferFormats.begin()),
                                std::to_address(gBufferFormats.end())),
      .DepthStencilFormat = Format::D32_Float};

  Axodox::Graphics::D3D12::PipelineState waterPipelineState =
      pipelineStateProvider_
          .CreatePipelineStateAsync(waterPipelineStateDefinition)
          .get();

  VertexShader atmosphereVS{app_folder() / L"AtmosphereVS.cso"};
  PixelShader atmospherePS{app_folder() / L"AtmospherePS.cso"};
  RootSignature<SkyboxRootDescription> skyboxRootSignature{device};
  DepthStencilState skyboxDepthStencilState{DepthStencilMode::WriteDepth};
  skyboxDepthStencilState.Comparison = ComparisonFunction::LessOrEqual;

  GraphicsPipelineStateDefinition skyboxPipelineStateDefinition{
      .RootSignature = &skyboxRootSignature,
      .VertexShader = &atmosphereVS,
      .PixelShader = &atmospherePS,
      .RasterizerState = RasterizerFlags::CullNone,
      .DepthStencilState = skyboxDepthStencilState,
      .InputLayout = VertexPositionNormalTexture::Layout,
      .RenderTargetFormats =
          std::initializer_list(std::to_address(gBufferFormats.begin()),
                                std::to_address(gBufferFormats.end())),

      .DepthStencilFormat = Format::D32_Float};
  Axodox::Graphics::D3D12::PipelineState skyboxPipelineState =
      pipelineStateProvider_
          .CreatePipelineStateAsync(skyboxPipelineStateDefinition)
          .get();

  VertexShader deferredShadingVS{app_folder() / L"DeferredShadingVS.cso"};
  PixelShader deferredShadingPS{app_folder() / L"DeferredShadingPS.cso"};
  RootSignature<DeferredShading> deferredShadingRootSignature{device};

  GraphicsPipelineStateDefinition deferredShadingPipelineStateDefinition{
      .RootSignature = &deferredShadingRootSignature,
      .VertexShader = &deferredShadingVS,
      .PixelShader = &deferredShadingPS,
      .BlendState = {BlendType::Additive, BlendType::AlphaBlend},
      .RasterizerState = RasterizerFlags::CullCounterClockwise,
      .InputLayout = VertexPositionNormalTexture::Layout,
      .TopologyType = PrimitiveTopologyType::Triangle,
      .RenderTargetFormats = {Format::B8G8R8A8_UNorm},
  };

  Axodox::Graphics::D3D12::PipelineState deferredShadingPipelineState =
      pipelineStateProvider_
          .CreatePipelineStateAsync(deferredShadingPipelineStateDefinition)
          .get();

  PostProcessingShader postProcessingShader =
      PostProcessingShader::WithDefaultShaders(pipelineStateProvider_, device);

  BasicShader basicShader =
      BasicShader::WithDefaultShaders(pipelineStateProvider_, device);

  // SilhouetteDetector silhouetteDetector =
  //     SilhouetteDetector::WithDefaultShaders(pipelineStateProvider,
  //     device);

  // SilhouetteClear silhouetteClear =
  //     SilhouetteClear::WithDefaultShaders(pipelineStateProvider, device);

  // SilhouetteDetectorTester silhouetteTester =
  //     SilhouetteDetectorTester::WithDefaultShaders(pipelineStateProvider,
  //                                                  device);

  ParallaxDraw parallaxDraw =
      ParallaxDraw::WithDefaultShaders(pipelineStateProvider_, device);

  PrismParallaxDraw prismParallaxDraw =
      PrismParallaxDraw::WithDefaultShaders(pipelineStateProvider_, device);

  return WaterRenderPipelines{
      .waterRootSignature = waterRootSignature,
      .waterPipelineStateDefinition = waterPipelineStateDefinition,
      .waterPipelineState = waterPipelineState,
      .skyboxRootSignature = skyboxRootSignature,
      .skyboxPipelineStateDefinition = skyboxPipelineStateDefinition,
      .skyboxPipelineState = skyboxPipelineState,
      .deferredShadingRootSignature = deferredShadingRootSignature,
      .deferredShadingPipelineStateDefinition =
          deferredShadingPipelineStateDefinition,
      .deferredShadingPipelineState = deferredShadingPipelineState,
      .postProcessingShader = postProcessingShader,
      .basicShader = basicShader,
      //. silhouetteDetector=silhouetteDetector  ,

      // . silhouetteClear= silhouetteClear ,

      // . silhouetteTester=silhouetteTester  ,
      .parallaxDraw = parallaxDraw,
      .prismParallaxDraw = prismParallaxDraw};
}

void WaterRenderPipelines::Execute(RenderFrameContext &context) {
  auto &frameResource = context.frameResources;
  auto &allocator = frameResource.Allocator;
  auto &constantBuffers = context.constantBuffers;
  auto &globalBuffers = context.globalBuffers;
  auto &debugValues = context.others.debugValues;
  auto &drawingSimResource = context.drawingSimResource;

  // Will be accessed in multiple sections
  const XMMATRIX &modelMatrix = context.others.oceanModelMatrix;

  // Need to reset after drawing
  std::optional<ShaderResourceView *> usedTextureAddress;
  std::optional<MutableTextureWithState *> usedTexture;

  // Start Draw pass
  // Debug Data
  {
    if (debugValues.debugTextureMode.has_value()) {
      const auto val = debugValues.debugTextureMode.value();
      switch (val) {
      case DebugValues::DebugTextureDisplay::DisplacementHighest:
        usedTexture = &drawingSimResource.HighestBuffer.displacementMap;
        break;
      case DebugValues::DebugTextureDisplay::GradientsHighest:
        usedTexture = &drawingSimResource.HighestBuffer.gradients;
        break;
      case DebugValues::DebugTextureDisplay::DisplacementMedium:
        usedTexture = &drawingSimResource.MediumBuffer.displacementMap;
        break;
      case DebugValues::DebugTextureDisplay::GradientsMedium:
        usedTexture = &drawingSimResource.MediumBuffer.gradients;
        break;
      case DebugValues::DebugTextureDisplay::DisplacementLowest:
        usedTexture = &drawingSimResource.LowestBuffer.displacementMap;
        break;
      case DebugValues::DebugTextureDisplay::GradientsLowest:
        usedTexture = &drawingSimResource.LowestBuffer.gradients;
        break;
      }
    }

    if (usedTexture.has_value()) {
      usedTextureAddress = (*usedTexture)->ShaderResource(allocator);
    }
  }

  GpuVirtualAddress waterDataBuffer =
      frameResource.DynamicBuffer.AddBuffer(constantBuffers.waterData);

  // Pre translate resources
  GpuVirtualAddress displacementMapAddressHighest =
      *drawingSimResource.HighestBuffer.displacementMap.ShaderResource(
          allocator);
  GpuVirtualAddress gradientsAddressHighest =
      *drawingSimResource.HighestBuffer.gradients.ShaderResource(allocator);
  GpuVirtualAddress displacementMapAddressMedium =
      *drawingSimResource.MediumBuffer.displacementMap.ShaderResource(
          allocator);
  GpuVirtualAddress gradientsAddressMedium =
      *drawingSimResource.MediumBuffer.gradients.ShaderResource(allocator);
  GpuVirtualAddress displacementMapAddressLowest =
      *drawingSimResource.LowestBuffer.displacementMap.ShaderResource(
          allocator);
  GpuVirtualAddress gradientsAddressLowest =
      *drawingSimResource.LowestBuffer.gradients.ShaderResource(allocator);

  // Shadow Map pass
  //{
  //  // Get shadow casting object silhouette
  //  {
  //    {
  //      silhouetteClear.Pre(allocator);
  //      SilhouetteClear::Inp inp{
  //          .buffers = silhouetteDetectorBuffers,
  //      };
  //      silhouetteClear.Run(allocator, frameResource.DynamicBuffer,
  //                          inp);
  //    }
  //    {
  //      silhouetteDetector.Pre(allocator);
  //      SilhouetteDetector::Inp inp{
  //          .buffers = silhouetteDetectorBuffers,
  //          .lights = lightsConstantBuffer,
  //          .mesh = Box,
  //          .meshBuffers = silhouetteDetectorMeshBuffers,
  //      };
  //      silhouetteDetector.Run(allocator, frameResource.DynamicBuffer,
  //                             inp);
  //    }
  //  }
  //  // ...
  //}

  // GBuffer Pass
  {
    auto gBufferViews = frameResource.GBuffer.GetGBufferViews();
    allocator.SetRenderTargets(
        std::initializer_list(std::to_address(gBufferViews.begin()),
                              std::to_address(gBufferViews.end())),
        frameResource.DepthBuffer.DepthStencil());

    // Box
    // outline
    //{
    //  allocator.TransitionResource(
    //      silhouetteDetectorBuffers.EdgeCountBuffer.get()->get(),
    //      ResourceStates::UnorderedAccess,
    //      ResourceStates::IndirectArgument);

    //  silhouetteTester.Pre(allocator);
    //  XMMATRIX boxModel = XMMatrixTranspose(
    //      XMMatrixTranslationFromVector(XMVECTOR{2, 5, 2, 0}));
    //  SilhouetteDetectorTester::ModelConstants boxModelConstants{};
    //  XMStoreFloat4x4(&boxModelConstants.mMatrix, boxModel);
    //  SilhouetteDetectorTester::Inp inp{
    //      .camera = cameraConstantBuffer,
    //      .modelTransform =
    //          frameResource.DynamicBuffer.AddBuffer(boxModelConstants),
    //      .texture = std::nullopt,
    //      .mesh = Box,
    //      .buffers = silhouetteDetectorBuffers,
    //      .meshBuffers = silhouetteDetectorMeshBuffers,
    //  };
    //  silhouetteTester.Run(allocator, frameResource.DynamicBuffer,
    //  inp); allocator.TransitionResource(
    //      silhouetteDetectorBuffers.EdgeCountBuffer.get()->get(),
    //      ResourceStates::IndirectArgument,
    //      ResourceStates::UnorderedAccess);
    //}
    // Box

    /*{
    basicShader.Pre(allocator);

    XMMATRIX boxModel = XMMatrixTranspose(
    XMMatrixTranslationFromVector(XMVECTOR{2, 5, 2, 0}));
    BasicShader::ShaderMask::ModelConstants boxModelConstants{};
    XMStoreFloat4x4(&boxModelConstants.mMatrix, boxModel);
    BasicShader::Inp inp{
    .camera = cameraConstantBuffer,
    .modelTransform =
    frameResource.DynamicBuffer.AddBuffer(boxModelConstants),
    .texture = std::nullopt,
    .mesh = Box,
    };
    basicShader.Run(allocator, frameResource.DynamicBuffer, inp);
    }*/

    // Water
    {
      if (debugValues.drawMethod == DebugValues::DrawTechnology::Tesselation) {

        // Ocean Buffers
        WaterGraphicRootDescription::ModelConstants modelConstants{};

        XMStoreFloat4x4(&modelConstants.mMatrix,
                        XMMatrixTranspose(modelMatrix));

        GpuVirtualAddress modelBuffer =
            frameResource.DynamicBuffer.AddBuffer(modelConstants);

        waterPipelineState.Apply(allocator);

        const auto &oceanQuadData = context.others.oceanDataFuture.get();
        for (auto &curr : oceanQuadData) {
          if (curr.N == 0)
            continue;
          auto mask =
              waterRootSignature.Set(allocator, RootSignatureUsage::Graphics);

          if (usedTextureAddress.has_value())
            mask.texture = **usedTextureAddress;

          mask.heightMapHighest = displacementMapAddressHighest;
          mask.gradientsHighest = gradientsAddressHighest;
          mask.heightMapMedium = displacementMapAddressMedium;
          mask.gradientsMedium = gradientsAddressMedium;
          mask.heightMapLowest = displacementMapAddressLowest;
          mask.gradientsLowest = gradientsAddressLowest;

          mask.waterPBRBuffer = waterDataBuffer;

          mask.hullBuffer =
              frameResource.DynamicBuffer.AddBuffer(curr.hullConstants);
          mask.vertexBuffer =
              frameResource.DynamicBuffer.AddBuffer(curr.vertexConstants);
          mask.debugBuffer = globalBuffers.debugConstantBuffer;
          mask.cameraBuffer = globalBuffers.cameraConstantBuffer;
          mask.modelBuffer = modelBuffer;

          context.meshes.planeMesh.Draw(allocator, curr.N);
        }
      } else if (debugValues.drawMethod ==
                 DebugValues::DrawTechnology::Parallax) {

        // Ocean Buffers
        ParallaxDraw::ModelBuffers modelConstants{};

        modelConstants.center = float3(0, -5, 0);
        modelConstants.scale = float2(DefaultsValues::App::oceanSize / 2,
                                      DefaultsValues::App::oceanSize / 2);
        modelConstants.PrismHeight = debugValues.prismHeight;

        GpuVirtualAddress modelBuffer =
            frameResource.DynamicBuffer.AddBuffer(modelConstants);

        parallaxDraw.Pre(allocator);

        ParallaxDraw::Inp inp{
            .coneMaps =
                {drawingSimResource.LODs[0]->coneMapBuffer.ShaderResource(
                     allocator),
                 drawingSimResource.LODs[1]->coneMapBuffer.ShaderResource(
                     allocator),
                 drawingSimResource.LODs[2]->coneMapBuffer.ShaderResource(
                     allocator)},
            .gradients =
                {
                    drawingSimResource.LODs[0]->gradients.ShaderResource(
                        allocator),
                    drawingSimResource.LODs[1]->gradients.ShaderResource(
                        allocator),
                    drawingSimResource.LODs[2]->gradients.ShaderResource(
                        allocator),

                },
            .texture = usedTextureAddress,
            .modelBuffers = modelBuffer,
            .cameraBuffer = globalBuffers.cameraConstantBuffer,
            .debugBuffers = globalBuffers.debugConstantBuffer,
            .waterPBRBuffers = waterDataBuffer,
            .mesh = context.meshes.simplePlane,

        };
        parallaxDraw.Run(allocator, inp);
      }

      else if (debugValues.drawMethod ==
               DebugValues::DrawTechnology::PrismParallax) {

        // Ocean Buffers
        PrismParallaxDraw::ModelBuffers modelConstants{};

        XMStoreFloat4x4(&modelConstants.mMatrix,
                        XMMatrixTranspose(modelMatrix));

        XMStoreFloat4x4(
            &modelConstants.mINVMatrix,
            XMMatrixTranspose(XMMatrixInverse(nullptr, modelMatrix)));

        modelConstants.center = XMFLOAT3{0, -5, 0};
        modelConstants.PrismHeight = debugValues.prismHeight;

        GpuVirtualAddress modelBuffer =
            frameResource.DynamicBuffer.AddBuffer(modelConstants);

        prismParallaxDraw.Pre(allocator);
        PrismParallaxDraw::Inp inp{
            .coneMaps =
                {drawingSimResource.LODs[0]->coneMapBuffer.ShaderResource(
                     allocator),
                 drawingSimResource.LODs[1]->coneMapBuffer.ShaderResource(
                     allocator),
                 drawingSimResource.LODs[2]->coneMapBuffer.ShaderResource(
                     allocator)},
            .gradients =
                {
                    drawingSimResource.LODs[0]->gradients.ShaderResource(
                        allocator),
                    drawingSimResource.LODs[1]->gradients.ShaderResource(
                        allocator),
                    drawingSimResource.LODs[2]->gradients.ShaderResource(
                        allocator),
                },
            .texture = usedTextureAddress,
            .cameraBuffer = globalBuffers.cameraConstantBuffer,
            .debugBuffers = globalBuffers.debugConstantBuffer,
            .waterPBRBuffers = waterDataBuffer,
            .modelBuffers = modelBuffer,
            //.mesh = BoxOnlyWithIndexBuffer,
            .mesh = context.meshes.BoxWithoutBottom,
            .vertexData = GpuVirtualAddress(0),
        };

        const auto &oceanQuadData = context.others.oceanDataFuture.get();
        for (auto &curr : oceanQuadData) {
          if (curr.N == 0)
            continue;

          inp.vertexData =
              frameResource.DynamicBuffer.AddBuffer(curr.vertexConstants);
          inp.N = curr.N;
          prismParallaxDraw.Run(allocator, inp);
        }
      }
    }
    // skybox
    {
      skyboxPipelineState.Apply(allocator);

      auto mask =
          skyboxRootSignature.Set(allocator, RootSignatureUsage::Graphics);

      mask.skybox = context.textures.skyboxTexture;
      mask.lightingBuffer = globalBuffers.lightsConstantBuffer;

      mask.cameraBuffer = globalBuffers.cameraConstantBuffer;

      context.meshes.skyboxMesh.Draw(allocator);
    }
  }

  // Deferred Shading Pass
  {
    allocator.SetRenderTargets({context.renderTargetView}, nullptr);
    deferredShadingPipelineState.Apply(allocator);
    frameResource.GBuffer.TranslateToView(allocator);
    allocator.TransitionResource(
        frameResource.DepthBuffer
            .operator Axodox::Graphics::D3D12::ResourceArgument(),
        ResourceStates::DepthWrite, ResourceStates::PixelShaderResource);
    auto mask = deferredShadingRootSignature.Set(allocator,
                                                 RootSignatureUsage::Graphics);

    mask.BindGBuffer(frameResource.GBuffer);

    // Textures
    mask.skybox = context.textures.skyboxTexture;
    mask.gradientsHighest =
        *drawingSimResource.HighestBuffer.gradients.ShaderResource(allocator);
    mask.gradientsMedium =
        *drawingSimResource.MediumBuffer.gradients.ShaderResource(allocator);
    mask.gradientsLowest =
        *drawingSimResource.LowestBuffer.gradients.ShaderResource(allocator);

    // Buffers
    mask.lightingBuffer = context.globalBuffers.lightsConstantBuffer;
    mask.cameraBuffer = context.globalBuffers.cameraConstantBuffer;
    mask.debugBuffer = context.globalBuffers.debugConstantBuffer;

    mask.deferredShaderBuffer = context.constantBuffers.defData;

    mask.geometryDepth = *frameResource.DepthBuffer.ShaderResource();

    context.meshes.deferredShadingPlane.Draw(allocator);
  }

  // SSR post process
  if (debugValues.enableSSR) {
    allocator.TransitionResource(*context.renderTargetView,
                                 ResourceStates::RenderTarget,
                                 ResourceStates::NonPixelShaderResource);

    auto definition = frameResource.PostProcessingBuffer.Definition();

    PostProcessingShader::Inp inp{
        .camera = context.globalBuffers.cameraConstantBuffer,
        .inp = *frameResource.ScreenResourceView,
        .depthBuffer = *frameResource.DepthBuffer.ShaderResource(),
        .normalBuffer = *frameResource.GBuffer.Normal.ShaderResource(),
        .textureBuffer = *frameResource.PostProcessingBuffer.ShaderResource(),
        .X = definition->Width / 16 + 1,
        .Y = definition->Height / 16 + 1,

        .output = *context.renderTargetView,
    };

    postProcessingShader.Pre(allocator);
    postProcessingShader.Run(allocator, frameResource.DynamicBuffer, inp);
  }

  allocator.TransitionResource(frameResource.DepthBuffer,
                               ResourceStates::PixelShaderResource,
                               ResourceStates::DepthWrite);
  frameResource.GBuffer.TranslateToTarget(allocator);

  // Retransition simulation resources for compute shaders

  drawingSimResource.HighestBuffer.gradients.UnorderedAccess(allocator);
  drawingSimResource.HighestBuffer.displacementMap.UnorderedAccess(allocator);
  drawingSimResource.MediumBuffer.gradients.UnorderedAccess(allocator);
  drawingSimResource.MediumBuffer.displacementMap.UnorderedAccess(allocator);
  drawingSimResource.LowestBuffer.gradients.UnorderedAccess(allocator);
  drawingSimResource.LowestBuffer.displacementMap.UnorderedAccess(allocator);
  if (usedTexture.has_value())
    (*usedTexture)->UnorderedAccess(allocator);
}
} // namespace Reun::Graphics
