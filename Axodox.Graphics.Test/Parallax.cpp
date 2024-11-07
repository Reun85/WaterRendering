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
  for (u32 i = 0; i < inp.mipLevels; ++i) {
    auto mask = Signature.Set(allocator, RootSignatureUsage::Compute);
    assert("not yet implemented");
    allocator.Dispatch((inp.Extent >> i) / 16, (inp.Extent >> i) / 16, 1);
  }
}
