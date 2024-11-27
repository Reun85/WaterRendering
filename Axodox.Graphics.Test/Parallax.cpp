#include "pch.h"
#include "Parallax.h"
#include "GraphicsPipeline.h"

namespace SimulationStage {
ConeMapCreater::ConeMapCreater(PipelineStateProvider &pipelineProvider,
                               GraphicsDevice &device, ComputeShader *cs)
    : Signature(device),
      pipeline(pipelineProvider
                   .CreatePipelineStateAsync(ComputePipelineStateDefinition{
                       .RootSignature = &Signature,
                       .ComputeShader = cs,
                   })
                   .get()) {}

ConeMapCreater
ConeMapCreater::WithDefaultShaders(PipelineStateProvider &pipelineProvider,
                                   GraphicsDevice &device) {
  ComputeShader cs(app_folder() / L"ConeCreater.cso");
  return ConeMapCreater(pipelineProvider, device, &cs);
}

void ConeMapCreater::Run(CommandAllocator &allocator, DynamicBufferManager &_,
                         const Inp &inp) const {
  auto mask = Signature.Set(allocator, RootSignatureUsage::Compute);

  mask.ConeMap = *inp.coneMap;
  mask.HeightMap = *inp.heightMap;
  mask.ComputeConstants = inp.ComputeConstants;

  const auto xGroupSize = 16;
  const auto yGroupSize = 16;
  const auto sizeX = inp.N;
  const auto sizeY = inp.N;
  allocator.Dispatch((sizeX + xGroupSize - 1) / xGroupSize,
                     (sizeY + yGroupSize - 1) / yGroupSize, 1);
}
ConeMapCreater2::ConeMapCreater2(PipelineStateProvider &pipelineProvider,
                                 GraphicsDevice &device, ComputeShader *cs)
    : Signature(device),
      pipeline(pipelineProvider
                   .CreatePipelineStateAsync(ComputePipelineStateDefinition{
                       .RootSignature = &Signature,
                       .ComputeShader = cs,
                   })
                   .get()) {}

ConeMapCreater2
ConeMapCreater2::WithDefaultShaders(PipelineStateProvider &pipelineProvider,
                                    GraphicsDevice &device) {
  ComputeShader cs(app_folder() / L"ConeCreater2.cso");
  return ConeMapCreater2(pipelineProvider, device, &cs);
}
void ConeMapCreater2::Run(CommandAllocator &allocator,
                          DynamicBufferManager &buffermanager,
                          const Inp &inp) const {

  auto mask = Signature.Set(allocator, RootSignatureUsage::Compute);

  mask.ConeMap = *inp.coneMap;
  mask.MixMax = *inp.mixMax;
  ConeMapCreater2::ShaderMask::Constants cont{
      .maxLevel = 2,
      .textureSize = {inp.N, inp.N},
      .deltaHalf = {0.5f / float(inp.N), 0.5f / float(inp.N)}};
  mask.ComputeConstants = buffermanager.AddBuffer(cont);

  const auto xGroupSize = 16;
  const auto yGroupSize = 16;
  const auto sizeX = inp.N;
  const auto sizeY = inp.N;
  allocator.Dispatch((sizeX + xGroupSize - 1) / xGroupSize,
                     (sizeY + yGroupSize - 1) / yGroupSize, 1);
}
} // namespace SimulationStage

ParallaxDraw::ParallaxDraw(PipelineStateProvider &pipelineProvider,
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

ParallaxDraw
ParallaxDraw::WithDefaultShaders(PipelineStateProvider &pipelineProvider,
                                 GraphicsDevice &device) {
  VertexShader vs(app_folder() / L"ParallaxVS.cso");
  PixelShader ps(app_folder() / L"ParallaxPS.cso");
  return ParallaxDraw(pipelineProvider, device, &vs, &ps);
}

void ParallaxDraw::Run(CommandAllocator &allocator, const Inp &inp) const {
  auto mask = Signature.Set(allocator, RootSignatureUsage::Graphics);

  if (inp.texture)
    mask.texture = **inp.texture;
  mask.coneMapHighest = *inp.coneMaps[0];
  mask.coneMapMedium = *inp.coneMaps[1];
  mask.coneMapLowest = *inp.coneMaps[2];
  mask.gradientsHighest = *inp.gradients[0];
  mask.gradientsMedium = *inp.gradients[1];
  mask.gradientsLowest = *inp.gradients[2];

  mask.waterPBRBuffer = inp.waterPBRBuffers;

  mask.debugBuffer = inp.debugBuffers;
  mask.cameraBuffer = inp.cameraBuffer;
  mask.modelBuffer = inp.modelBuffers;

  inp.mesh.Draw(allocator);
}

MixMaxCompute::MixMaxCompute(PipelineStateProvider &pipelineProvider,
                             GraphicsDevice &device, ComputeShader *cs)

    : Signature(device),
      pipeline(pipelineProvider
                   .CreatePipelineStateAsync(ComputePipelineStateDefinition{
                       .RootSignature = &Signature,
                       .ComputeShader = cs,
                   })
                   .get()) {}

MixMaxCompute
MixMaxCompute::WithDefaultShaders(PipelineStateProvider &pipelineProvider,
                                  GraphicsDevice &device) {
  ComputeShader cs(app_folder() / L"MixMax.cso");
  return MixMaxCompute(pipelineProvider, device, &cs);
}

void MixMaxCompute::Run(CommandAllocator &allocator,
                        DynamicBufferManager &buffermanager,
                        const Inp &inp) const {
  auto mask = Signature.Set(allocator, RootSignatureUsage::Compute);
  Buffer buffers{.SrcMipLevel = 0,
                 .NumMipLevels = inp.mipLevels,
                 .TexelSize = float2(1., 1.) / float2(inp.Extent, inp.Extent)};
  mask.ComputeConstants = buffermanager.AddBuffer(buffers);
  mask.ReadTexture = *inp.texture;
  mask.MipMap1 = *inp.mipMaps[0];
  mask.MipMap2 = *inp.mipMaps[1];
  mask.MipMap3 = *inp.mipMaps[2];
  mask.MipMap4 = *inp.mipMaps[3];
  allocator.Dispatch(inp.Extent >> 3, inp.Extent >> 3, 1);
}

PrismParallaxDraw::PrismParallaxDraw(PipelineStateProvider &pipelineProvider,
                                     GraphicsDevice &device, VertexShader *vs,
                                     PixelShader *ps)
    : Signature(device),
      pipeline(
          pipelineProvider
              .CreatePipelineStateAsync(GraphicsPipelineStateDefinition{
                  .RootSignature = &Signature,
                  .VertexShader = vs,
                  .PixelShader = ps,
                  // .RasterizerState = RasterizerFlags::CullNone,
                  .RasterizerState = RasterizerFlags::CullClockwise,
                  //.RasterizerState = RasterizerFlags::Wireframe,
                  .DepthStencilState = DepthStencilMode::WriteDepth,
                  .InputLayout = VertexPositionNormalTexture::Layout,
                  .TopologyType = PrimitiveTopologyType::Triangle,
                  .RenderTargetFormats = std::initializer_list(
                      std::to_address(
                          DeferredShading::GBuffer::GetGBufferFormats()
                              .begin()),
                      std::to_address(
                          DeferredShading::GBuffer::GetGBufferFormats().end())),
                  .DepthStencilFormat = Format::D32_Float})
              .get()) {}

PrismParallaxDraw
PrismParallaxDraw::WithDefaultShaders(PipelineStateProvider &pipelineProvider,
                                      GraphicsDevice &device) {
  VertexShader vs(app_folder() / L"PrismParallaxVS.cso");
  PixelShader ps(app_folder() / L"PrismParallaxPS.cso");
  return PrismParallaxDraw(pipelineProvider, device, &vs, &ps);
}

void PrismParallaxDraw::Run(CommandAllocator &allocator, const Inp &inp) const {
  auto mask = Signature.Set(allocator, RootSignatureUsage::Graphics);

  if (inp.texture)
    mask.texture = **inp.texture;

  mask.coneMapHighest = *inp.coneMaps[0];
  mask.coneMapMedium = *inp.coneMaps[1];
  mask.coneMapLowest = *inp.coneMaps[2];

  mask.gradientsHighest = *inp.gradients[0];
  mask.gradientsMedium = *inp.gradients[1];
  mask.gradientsLowest = *inp.gradients[2];

  mask.heightMapHighest = *inp.heightMaps[0];
  mask.heightMapMedium = *inp.heightMaps[1];
  mask.heightMapLowest = *inp.heightMaps[2];

  mask.waterPBRBuffer = inp.waterPBRBuffers;

  mask.debugBuffer = inp.debugBuffers;
  mask.cameraBuffer = inp.cameraBuffer;
  mask.modelBuffer = inp.modelBuffers;
  mask.vertexBuffer = inp.vertexData;

  inp.mesh.Draw(allocator, inp.N);
}

DisplacedHeightMapJob::DisplacedHeightMapJob(
    PipelineStateProvider &pipelineProvider, GraphicsDevice &device,
    ComputeShader *cs)
    : Signature(device),
      pipeline(pipelineProvider
                   .CreatePipelineStateAsync(ComputePipelineStateDefinition{
                       .RootSignature = &Signature,
                       .ComputeShader = cs,
                   })
                   .get()) {}

DisplacedHeightMapJob DisplacedHeightMapJob::WithDefaultShaders(
    PipelineStateProvider &pipelineProvider, GraphicsDevice &device) {
  ComputeShader cs(app_folder() / L"displacementToHeightMap.cso");
  return DisplacedHeightMapJob(pipelineProvider, device, &cs);
}

void DisplacedHeightMapJob::Run(CommandAllocator &allocator,
                                DynamicBufferManager &buffermanager,
                                const Inp &inp) const {

  auto mask = Signature.Set(allocator, RootSignatureUsage::Compute);
  mask.ComputeConstants = inp.ComputeConstants;
  mask.DisplacementMap = *inp.displacementMap;
  mask.Gradients = *inp.gradients;
  mask.OutHeightMap = *inp.outHeightMap;
  mask.OutGradients = *inp.outGradients;
  allocator.Dispatch(inp.N, inp.N, 1);
}
