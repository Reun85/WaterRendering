#pragma once
#include "pch.h"
#include "Simulation.h"

using namespace std;
using namespace winrt;

using namespace Windows;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::Foundation::Numerics;
using namespace Windows::UI;
using namespace Windows::UI::Core;
using namespace Windows::UI::Composition;

using namespace Axodox::Graphics::D3D12;
using namespace Axodox::Infrastructure;
using namespace Axodox::Storage;
using namespace DirectX;
using namespace DirectX::PackedVector;

template <typename T, typename TypeA, typename TypeB>
concept Either = std::same_as<T, TypeA> || std::same_as<T, TypeB>;
struct SimulationStage {
  struct LODComputeBuffer {
    float4 displacementLambda;
    float patchSize;
    float foamExponentialDecay;
    float foamMinValue;
    float foamBias;
    float foamMult;

    explicit LODComputeBuffer(const SimulationData::PatchData &patchData)
        : displacementLambda(patchData.displacementLambda.z,
                             patchData.displacementLambda.y,
                             patchData.displacementLambda.z, 1),
          patchSize(patchData.patchSize),

          foamExponentialDecay(patchData.foamExponentialDecay),
          foamMinValue(patchData.foamMinValue), foamBias(patchData.foamBias),
          foamMult(patchData.foamMult)

    {}
  };
  struct SpektrumRootDescription : public RootSignatureMask {
    // In
    RootDescriptorTable<1> Tildeh0;
    RootDescriptorTable<1> Frequencies;
    RootDescriptor<RootDescriptorType::ConstantBuffer> timeDataBuffer;
    // Out
    RootDescriptorTable<1> Tildeh;
    RootDescriptorTable<1> TildeD;

    explicit SpektrumRootDescription(const RootSignatureContext &context)
        : RootSignatureMask(context),
          Tildeh0(this, {DescriptorRangeType::ShaderResource, 0}),
          Frequencies(this, {DescriptorRangeType::ShaderResource, 1}),
          timeDataBuffer(this, {0}),
          Tildeh(this, {DescriptorRangeType::UnorderedAccess, 0}),
          TildeD(this, {DescriptorRangeType::UnorderedAccess, 1}) {
      Flags = RootSignatureFlags::None;
    }
  };
  struct FFTDescription : public RootSignatureMask {
    // In
    RootDescriptorTable<1> Input;
    // Out
    RootDescriptorTable<1> Output;

    explicit FFTDescription(const RootSignatureContext &context)
        : RootSignatureMask(context),
          Input(this, {DescriptorRangeType::ShaderResource, 0}),
          Output(this, {DescriptorRangeType::UnorderedAccess, 0}) {
      Flags = RootSignatureFlags::None;
    }
  };
  struct DisplacementDescription : public RootSignatureMask {
    // In
    RootDescriptorTable<1> Height;
    RootDescriptorTable<1> Choppy;
    RootDescriptor<RootDescriptorType::ConstantBuffer> constantBuffer;

    // Out
    RootDescriptorTable<1> Output;

    explicit DisplacementDescription(const RootSignatureContext &context)
        : RootSignatureMask(context),
          Height(this, {DescriptorRangeType::ShaderResource, 0}),
          Choppy(this, {DescriptorRangeType::ShaderResource, 1}),
          constantBuffer(this, {9}),
          Output(this, {DescriptorRangeType::UnorderedAccess, 0}) {
      Flags = RootSignatureFlags::None;
    }
  };
  struct GradientDescription : public RootSignatureMask {
    // In
    RootDescriptorTable<1> Displacement;
    RootDescriptor<RootDescriptorType::ConstantBuffer> constantBuffer;
    // Out
    RootDescriptorTable<1> Output;

    explicit GradientDescription(const RootSignatureContext &context)
        : RootSignatureMask(context),
          Displacement(this, {DescriptorRangeType::ShaderResource, 0}),
          constantBuffer(this, {9}),
          Output(this, {DescriptorRangeType::UnorderedAccess, 0}) {
      Flags = RootSignatureFlags::None;
    }
  };
  struct FoamDecayDescription : public RootSignatureMask {
    // In-Out
    RootDescriptorTable<1> Gradients;
    RootDescriptor<RootDescriptorType::ConstantBuffer> constantBuffer;
    // In
    RootDescriptorTable<1> Foam;
    RootDescriptor<RootDescriptorType::ConstantBuffer> timeBuffer;

    explicit FoamDecayDescription(const RootSignatureContext &context)
        : RootSignatureMask(context),
          Gradients(this, {DescriptorRangeType::UnorderedAccess, 0}),
          constantBuffer(this, {9}),
          Foam(this, {DescriptorRangeType::UnorderedAccess, 1}),
          timeBuffer(this, {0}) {
      Flags = RootSignatureFlags::None;
    }
  };

  struct SimulationResources {
    CommandAllocator Allocator;
    CommandFence Fence;
    CommandFenceMarker FrameDoneMarker;
    DynamicBufferManager DynamicBuffer;

#define useDifferentFFTOutputBuffers true
    struct LODDataBuffers {
      MutableTextureWithState tildeh;
      MutableTextureWithState tildeD;
#if useDifferentFFTBuffers
      MutableTextureWithState FFTTildeh;
      MutableTextureWithState FFTTildeD;
#else
      MutableTextureWithState &FFTTildeh = tildeh;
      MutableTextureWithState &FFTTildeD = tildeD;
#endif
      MutableTextureWithState displacementMap;
      MutableTextureWithState gradients;

      MutableTextureWithState tildehBuffer;
      MutableTextureWithState tildeDBuffer;

      LODDataBuffers(const ResourceAllocationContext &context, const u32 N,
                     const u32 M)
          : tildeh(context, TextureDefinition::TextureDefinition(
                                Format::R32G32_Float, N, M, 0,
                                TextureFlags::UnorderedAccess)),
            tildeD(context, TextureDefinition::TextureDefinition(
                                Format::R32G32_Float, N, M, 0,
                                TextureFlags::UnorderedAccess)),
#if useDifferentFFTBuffers
            FFTTildeh(context, TextureDefinition::TextureDefinition(
                                   Format::R32G32_Float, N, M, 0,
                                   TextureFlags::UnorderedAccess)),
            FFTTildeD(context, TextureDefinition::TextureDefinition(
                                   Format::R32G32_Float, N, M, 0,
                                   TextureFlags::UnorderedAccess)),
#endif
            tildehBuffer(context, TextureDefinition::TextureDefinition(
                                      Format::R32G32_Float, N, M, 0,
                                      TextureFlags::UnorderedAccess)),
            tildeDBuffer(context, TextureDefinition::TextureDefinition(
                                      Format::R32G32_Float, N, M, 0,
                                      TextureFlags::UnorderedAccess)),
            displacementMap(context, TextureDefinition::TextureDefinition(
                                         Format::R16G16B16A16_Float, N, M, 0,
                                         TextureFlags::UnorderedAccess)),
            gradients(context, TextureDefinition::TextureDefinition(
                                   Format::R16G16B16A16_Float, N, M, 0,
                                   TextureFlags::UnorderedAccess)) {
      }
    };

    LODDataBuffers HighestBuffer;
    LODDataBuffers MediumBuffer;
    LODDataBuffers LowestBuffer;
    const std::array<const LODDataBuffers *const, 3> LODs = {
        &HighestBuffer, &MediumBuffer, &LowestBuffer};

    explicit SimulationResources(const ResourceAllocationContext &context,
                                 const u32 N, const u32 M)
        : Allocator(*context.Device),

          Fence(*context.Device), DynamicBuffer(*context.Device),
          HighestBuffer(context, N, M), MediumBuffer(context, N, M),
          LowestBuffer(context, N, M) {}
  };

  template <typename TextureTy = MutableTexture>
    requires Either<TextureTy, MutableTexture, ImmutableTexture>
  struct ConstantGpuSources {
    struct LODDataSource {
      TextureTy Tildeh0;
      TextureTy Frequencies;
      LODDataSource(ResourceAllocationContext &context,
                    const SimulationData::PatchData &inp)
          : Tildeh0(TextureTy(context, CreateTextureData<std::complex<f32>>(
                                           Format::R32G32_Float, inp.N, inp.M,
                                           0u, CalculateTildeh0<f32>(inp)))),
            Frequencies(TextureTy(
                context,
                CreateTextureData<f32>(Format::R32_Float, inp.N, inp.M, 0u,
                                       CalculateFrequencies<f32>(inp)))) {}
    };
    LODDataSource Highest;
    LODDataSource Medium;
    LODDataSource Lowest;
    const std::array<const LODDataSource *const, 3> LODs = {&Highest, &Medium,
                                                            &Lowest};
    ConstantGpuSources(ResourceAllocationContext &context,
                       const SimulationData &inp)
        : Highest(context, inp.Highest), Medium(context, inp.Medium),
          Lowest(context, inp.Lowest) {}
    // ImmutableTexture PerlinNoise;
  };

  struct MutableGpuSources {
    struct LODDataSource {
      MutableTexture Foam;
      LODDataSource(ResourceAllocationContext &context,
                    const SimulationData &simData)
          : Foam(MutableTexture(context,
                                TextureDefinition::TextureDefinition(
                                    Format::R32G32_Float, simData.N, simData.M,
                                    0, TextureFlags::UnorderedAccess))) {}
    };
    LODDataSource Highest;
    LODDataSource Medium;
    LODDataSource Lowest;
    MutableGpuSources(ResourceAllocationContext &context,
                      const SimulationData &simData)
        : Highest(context, simData), Medium(context, simData),
          Lowest(context, simData) {}
  };

  struct FullPipeline {
    RootSignature<SimulationStage::SpektrumRootDescription>
        spektrumRootDescription;
    RootSignature<SimulationStage::FFTDescription> FFTRootDescription;
    RootSignature<SimulationStage::DisplacementDescription>
        displacementRootDescription;
    RootSignature<SimulationStage::GradientDescription> gradientRootDescription;
    RootSignature<SimulationStage::FoamDecayDescription>
        foamDecayRootDescription;

    PipelineState spektrumPipeline;
    PipelineState FFTPipeline;
    PipelineState displacementPipeline;
    PipelineState gradientPipeline;
    PipelineState foamDecayPipeline;

    static FullPipeline Create(GraphicsDevice &device,
                               PipelineStateProvider &pipelineStateProvider) {
      union x {
        FullPipeline result;
      };

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
      auto spektrumPipelineState =
          pipelineStateProvider.CreatePipelineStateAsync(
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

      RootSignature<SimulationStage::GradientDescription>
          gradientRootDescription{device};
      ComputePipelineStateDefinition gradientRootStateDefinition{
          .RootSignature = &gradientRootDescription,
          .ComputeShader = &gradient,
      };
      auto gradientPipelineState =
          pipelineStateProvider.CreatePipelineStateAsync(
              gradientRootStateDefinition);
      RootSignature<SimulationStage::FoamDecayDescription>
          foamDecayRootDescription{device};
      ComputePipelineStateDefinition foamDecayRootStateDefinition{
          .RootSignature = &foamDecayRootDescription,
          .ComputeShader = &foamDecay,
      };
      auto foamDecayPipelineState =
          pipelineStateProvider.CreatePipelineStateAsync(
              foamDecayRootStateDefinition);

      return FullPipeline{
          .spektrumRootDescription = spektrumRootDescription,
          .FFTRootDescription = FFTRootDescription,
          .displacementRootDescription = displacementRootDescription,
          .gradientRootDescription = gradientRootDescription,
          .foamDecayRootDescription = foamDecayRootDescription,
          .spektrumPipeline = spektrumPipelineState.get(),
          .FFTPipeline = FFTPipelineState.get(),
          .displacementPipeline = displacementPipelineState.get(),
          .gradientPipeline = gradientPipelineState.get(),
          .foamDecayPipeline = foamDecayPipelineState.get(),
      };
    }
  };
};

static void WaterSimulationComputeShader(
    SimulationStage::SimulationResources &simResource,
    SimulationStage::ConstantGpuSources<Axodox::Graphics::D3D12::MutableTexture>
        &simulationConstantSources,
    SimulationStage::MutableGpuSources &simulationMutableSources,
    const SimulationData &simData,
    SimulationStage::FullPipeline &fullSimPipeline,
    Axodox::Graphics::D3D12::CommandAllocator &computeAllocator,
    Axodox::Graphics::D3D12::GpuVirtualAddress timeDataBuffer, const u32 &N,
    const std::array<bool, 3> useLod = {true, true, true}) {
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
      *dat.buffers.FFTTildeh.ShaderResource(computeAllocator);
      *dat.buffers.FFTTildeD.ShaderResource(computeAllocator);
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
      *dat.buffers.displacementMap.ShaderResource(computeAllocator);
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
}
