#pragma once
#include "pch.h"
#include "ComputePipeline.h"

namespace SimulationStage {
FullPipeline SimulationStage::FullPipeline::Create(
    GraphicsDevice &device, PipelineStateProvider &pipelineStateProvider) {

  ComputeShader spektrum{app_folder() / L"Spektrums.cso"};
  ComputeShader FFT{app_folder() / L"FFT.cso"};
  ComputeShader displacement{app_folder() / L"displacement.cso"};
  ComputeShader gradient{app_folder() / L"gradient.cso"};
  ComputeShader foamDecay{app_folder() / L"foamDecay.cso"};

  RootSignature<SimulationStage::SpektrumRootDescription>
      spektrumRootDescription{device};
  ComputePipelineStateDefinition spektrumRootStateDefinition{
      .RootSignature = &spektrumRootDescription,
      .ComputeShader = &spektrum,
  };
  auto spektrumPipelineState = pipelineStateProvider.CreatePipelineStateAsync(
      spektrumRootStateDefinition);

  RootSignature<SimulationStage::FFTDescription> FFTRootDescription{device};
  ComputePipelineStateDefinition FFTStateDefinition{
      .RootSignature = &FFTRootDescription,
      .ComputeShader = &FFT,
  };
  auto FFTPipelineState =
      pipelineStateProvider.CreatePipelineStateAsync(FFTStateDefinition);

  RootSignature<SimulationStage::DisplacementDescription>
      displacementRootDescription{device};
  ComputePipelineStateDefinition displacementRootStateDefinition{
      .RootSignature = &displacementRootDescription,
      .ComputeShader = &displacement,
  };
  auto displacementPipelineState =
      pipelineStateProvider.CreatePipelineStateAsync(
          displacementRootStateDefinition);

  RootSignature<SimulationStage::GradientDescription> gradientRootDescription{
      device};
  ComputePipelineStateDefinition gradientRootStateDefinition{
      .RootSignature = &gradientRootDescription,
      .ComputeShader = &gradient,
  };
  auto gradientPipelineState = pipelineStateProvider.CreatePipelineStateAsync(
      gradientRootStateDefinition);
  RootSignature<SimulationStage::FoamDecayDescription> foamDecayRootDescription{
      device};
  ComputePipelineStateDefinition foamDecayRootStateDefinition{
      .RootSignature = &foamDecayRootDescription,
      .ComputeShader = &foamDecay,
  };
  auto foamDecayPipelineState = pipelineStateProvider.CreatePipelineStateAsync(
      foamDecayRootStateDefinition);

  ConeMapCreater coneMapCreater =
      ConeMapCreater::WithDefaultShaders(pipelineStateProvider, device);

  return FullPipeline{.spektrumRootDescription = spektrumRootDescription,
                      .FFTRootDescription = FFTRootDescription,
                      .displacementRootDescription =
                          displacementRootDescription,
                      .gradientRootDescription = gradientRootDescription,
                      .foamDecayRootDescription = foamDecayRootDescription,
                      .spektrumPipeline = spektrumPipelineState.get(),
                      .FFTPipeline = FFTPipelineState.get(),
                      .displacementPipeline = displacementPipelineState.get(),
                      .gradientPipeline = gradientPipelineState.get(),
                      .foamDecayPipeline = foamDecayPipelineState.get(),
                      .coneMapCreater = coneMapCreater};
}

void SimulationStage::WaterSimulationComputeShader(
    SimulationStage::SimulationResources &simResource,
    SimulationStage::ConstantGpuSources<Axodox::Graphics::D3D12::MutableTexture>
        &simulationConstantSources,
    SimulationStage::MutableGpuSources &simulationMutableSources,
    const SimulationData &simData,
    SimulationStage::FullPipeline &fullSimPipeline,
    Axodox::Graphics::D3D12::CommandAllocator &computeAllocator,
    Axodox::Graphics::D3D12::GpuVirtualAddress timeDataBuffer, const u32 &N,
    const std::array<bool, 3> useLod) {
  struct LODData {
  public:
    SimulationStage::SimulationResources::LODDataBuffers &buffers;
    SimulationStage::ConstantGpuSources<>::LODDataSource &sources;
    MutableTexture &Foam;
    GpuVirtualAddress constantBuffer;
  };

  std::vector<LODData> lodData;
  if (useLod[0])
    lodData.emplace_back(
        simResource.HighestBuffer, simulationConstantSources.Highest,
        simulationMutableSources.Highest.Foam,
        simResource.DynamicBuffer.AddBuffer(
            SimulationStage::LODComputeBuffer(simData.Highest)));
  if (useLod[1])
    lodData.emplace_back(
        simResource.MediumBuffer, simulationConstantSources.Medium,
        simulationMutableSources.Medium.Foam,
        simResource.DynamicBuffer.AddBuffer(
            SimulationStage::LODComputeBuffer(simData.Medium)));

  if (useLod[2])
    lodData.emplace_back(
        simResource.LowestBuffer, simulationConstantSources.Lowest,
        simulationMutableSources.Lowest.Foam,
        simResource.DynamicBuffer.AddBuffer(
            SimulationStage::LODComputeBuffer(simData.Lowest)));

  // Spektrums
  fullSimPipeline.spektrumPipeline.Apply(computeAllocator);
  for (const LODData &dat : lodData) {
    auto mask = fullSimPipeline.spektrumRootDescription.Set(
        computeAllocator, RootSignatureUsage::Compute);
    // Inputs
    mask.timeDataBuffer = timeDataBuffer;

    mask.Tildeh0 = *dat.sources.Tildeh0.ShaderResource();
    mask.Frequencies = *dat.sources.Frequencies.ShaderResource();

    // Outputs
    mask.Tildeh = *dat.buffers.tildeh.UnorderedAccess(computeAllocator);
    mask.TildeD = *dat.buffers.tildeD.UnorderedAccess(computeAllocator);

    const auto xGroupSize = 16;
    const auto yGroupSize = 16;
    const auto sizeX = N;
    const auto sizeY = N;
    computeAllocator.Dispatch((sizeX + xGroupSize - 1) / xGroupSize,
                              (sizeY + yGroupSize - 1) / yGroupSize, 1);
  }
  //  FFT
  {
    const auto doFFT = [&computeAllocator, &N,
                        &fullSimPipeline](MutableTextureWithState &inp,
                                          MutableTextureWithState &out) {
      auto mask = fullSimPipeline.FFTRootDescription.Set(
          computeAllocator, RootSignatureUsage::Compute);

      mask.Input = *inp.ShaderResource(computeAllocator);
      mask.Output = *out.UnorderedAccess(computeAllocator);

      const auto xGroupSize = 1;
      const auto yGroupSize = 1;
      const auto sizeX = N;
      const auto sizeY = 1;
      computeAllocator.Dispatch((sizeX + xGroupSize - 1) / xGroupSize,
                                (sizeY + yGroupSize - 1) / yGroupSize, 1);
    };

    fullSimPipeline.FFTPipeline.Apply(computeAllocator);
    // UAV
    for (const LODData &dat : lodData) {
      MutableTextureWithState &text = dat.buffers.tildeh;
      computeAllocator.AddUAVBarrier(*text.UnorderedAccess(computeAllocator));

      MutableTextureWithState &text2 = dat.buffers.tildeD;
      computeAllocator.AddUAVBarrier(*text2.UnorderedAccess(computeAllocator));
    }
    // Transition
    for (const LODData &dat : lodData) {
      MutableTextureWithState &text = dat.buffers.tildeh;
      text.ShaderResource(computeAllocator);

      MutableTextureWithState &text2 = dat.buffers.tildeD;
      text2.ShaderResource(computeAllocator);
    }
    // Stage1
    for (const LODData &dat : lodData) {
      doFFT(dat.buffers.tildeh, dat.buffers.tildehBuffer);
      doFFT(dat.buffers.tildeD, dat.buffers.tildeDBuffer);
    }
    // UAV
    for (const LODData &dat : lodData) {
      MutableTextureWithState &text = dat.buffers.tildehBuffer;
      computeAllocator.AddUAVBarrier(*text.UnorderedAccess(computeAllocator));

      MutableTextureWithState &text2 = dat.buffers.tildeDBuffer;
      computeAllocator.AddUAVBarrier(*text2.UnorderedAccess(computeAllocator));
    }
    // Transition
    for (const LODData &dat : lodData) {
      MutableTextureWithState &text = dat.buffers.tildehBuffer;
      text.ShaderResource(computeAllocator);

      MutableTextureWithState &text2 = dat.buffers.tildeDBuffer;
      text2.ShaderResource(computeAllocator);
    }
    // Stage2
    for (const LODData &dat : lodData) {
      doFFT(dat.buffers.tildehBuffer, dat.buffers.FFTTildeh);
      doFFT(dat.buffers.tildeDBuffer, dat.buffers.FFTTildeD);
    }
  }

  // Calculate final displacements
  {
    fullSimPipeline.displacementPipeline.Apply(computeAllocator);
    // UAV
    for (const LODData &dat : lodData) {
      computeAllocator.AddUAVBarrier(
          *dat.buffers.FFTTildeh.UnorderedAccess(computeAllocator));
      computeAllocator.AddUAVBarrier(
          *dat.buffers.FFTTildeD.UnorderedAccess(computeAllocator));
    }
    for (const LODData &dat : lodData) {
      dat.buffers.FFTTildeh.ShaderResource(computeAllocator);
      dat.buffers.FFTTildeD.ShaderResource(computeAllocator);
    }
    for (const LODData &dat : lodData) {
      auto mask = fullSimPipeline.displacementRootDescription.Set(
          computeAllocator, RootSignatureUsage::Compute);

      mask.constantBuffer = dat.constantBuffer;
      mask.Height = *dat.buffers.FFTTildeh.ShaderResource(computeAllocator);
      mask.Choppy = *dat.buffers.FFTTildeD.ShaderResource(computeAllocator);
      mask.Output =
          *dat.buffers.displacementMap.UnorderedAccess(computeAllocator);

      const auto xGroupSize = 16;
      const auto yGroupSize = 16;
      const auto sizeX = N;
      const auto sizeY = N;
      computeAllocator.Dispatch((sizeX + xGroupSize - 1) / xGroupSize,
                                (sizeY + yGroupSize - 1) / yGroupSize, 1);
    }
  }

  // Calculate gradients
  {
    fullSimPipeline.gradientPipeline.Apply(computeAllocator);
    // UAV
    for (const LODData &dat : lodData) {
      computeAllocator.AddUAVBarrier(
          *dat.buffers.displacementMap.UnorderedAccess(computeAllocator));
    }
    // Transition
    for (const LODData &dat : lodData) {
      dat.buffers.displacementMap.ShaderResource(computeAllocator);
    }
    for (const LODData &dat : lodData) {
      auto mask = fullSimPipeline.gradientRootDescription.Set(
          computeAllocator, RootSignatureUsage::Compute);

      mask.constantBuffer = dat.constantBuffer;
      mask.Displacement =
          *dat.buffers.displacementMap.ShaderResource(computeAllocator);
      mask.Output = *dat.buffers.gradients.UnorderedAccess(computeAllocator);

      const auto xGroupSize = 16;
      const auto yGroupSize = 16;
      const auto sizeX = N;
      const auto sizeY = N;
      computeAllocator.Dispatch((sizeX + xGroupSize - 1) / xGroupSize,
                                (sizeY + yGroupSize - 1) / yGroupSize, 1);
    }
  }
  // Foam Calculations

  {
    fullSimPipeline.foamDecayPipeline.Apply(computeAllocator);
    for (const LODData &dat : lodData) {
      computeAllocator.AddUAVBarrier(
          *dat.buffers.gradients.UnorderedAccess(computeAllocator));
    }
    for (const LODData &dat : lodData) {
      auto mask = fullSimPipeline.foamDecayRootDescription.Set(
          computeAllocator, RootSignatureUsage::Compute);

      mask.constantBuffer = dat.constantBuffer;
      mask.Gradients = *dat.buffers.gradients.UnorderedAccess(computeAllocator);

      mask.Foam = *dat.Foam.UnorderedAccess();
      mask.timeBuffer = timeDataBuffer;

      const auto xGroupSize = 16;
      const auto yGroupSize = 16;
      const auto sizeX = N;
      const auto sizeY = N;
      computeAllocator.Dispatch((sizeX + xGroupSize - 1) / xGroupSize,
                                (sizeY + yGroupSize - 1) / yGroupSize, 1);
    }
  }

  // Create ConeMaps
  {
    fullSimPipeline.coneMapCreater.Pre(computeAllocator);
    for (const LODData &dat : lodData) {
      fullSimPipeline.coneMapCreater.Run(
          computeAllocator, simResource.DynamicBuffer,
          {dat.buffers.coneMapBuffer.UnorderedAccess(computeAllocator),
           dat.buffers.displacementMap.ShaderResource(computeAllocator),
           dat.constantBuffer, N});
    }
  }
}
} // namespace SimulationStage
