#include "pch.h"
#include "Parallax.h"

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
