#pragma once

#include "pch.h"
#include "Simulation.h"
#include "Helpers.h"
#include "Parallax.h"
#include "DebugValues.h"

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
namespace SimulationStage {
struct ConeMapCreater;

template <typename T, typename TypeA, typename TypeB>
concept Either = std::same_as<T, TypeA> || std::same_as<T, TypeB>;
struct LODComputeBuffer {
  float4 displacementLambda;
  float patchSize;
  float foamExponentialDecay;
  float foamMinValue;
  float foamBias;
  float foamMult;

  explicit LODComputeBuffer(const SimulationData::PatchData &patchData)
      : displacementLambda(patchData.displacementLambda.x,
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

#define useDifferentFFTOutputBuffers false
  struct LODDataBuffers {
    MutableTextureWithState tildeh;
    MutableTextureWithState tildeD;
    MutableTextureWithState displacementMap;
    MutableTextureWithState gradients;
    MutableTextureWithState Dim2HighBuffer1;
    MutableTextureWithState Dim2HighBuffer2;

    // References
#if useDifferentFFTBuffers
    MutableTextureWithState FFTTildeh;
    MutableTextureWithState FFTTildeD;
#else
    MutableTextureWithState &FFTTildeh = tildeh;
    MutableTextureWithState &FFTTildeD = tildeD;
#endif

    MutableTextureWithState &tildehBuffer = Dim2HighBuffer1;
    MutableTextureWithState &tildeDBuffer = Dim2HighBuffer2;

    MutableTextureWithState &coneMapBuffer = Dim2HighBuffer1;

    MutableTextureWithState mixMaxDisplacementMap;
    std::array<UnorderedAccessViewRef, 4> DisplacementMapMipsUAV;

    Axodox::Infrastructure::event_subscription _allocatedSubscription;

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
          Dim2HighBuffer1(context, TextureDefinition::TextureDefinition(
                                       Format::R32G32_Float, N, M, 0,
                                       TextureFlags::UnorderedAccess)),
          Dim2HighBuffer2(context, TextureDefinition::TextureDefinition(
                                       Format::R32G32_Float, N, M, 0,
                                       TextureFlags::UnorderedAccess)),
          displacementMap(context, TextureDefinition::TextureDefinition(
                                       Format::R16G16B16A16_Float, N, M, 0,
                                       TextureFlags::UnorderedAccess)),
          mixMaxDisplacementMap(
              context, TextureDefinition::TextureDefinition(
                           TextureHeader::TextureHeader(D3D12_RESOURCE_DESC{
                               D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                               0,
                               N,
                               N,
                               0,
                               4,
                               DXGI_FORMAT_R16G16_FLOAT,
                               {0, 0},
                               D3D12_TEXTURE_LAYOUT_UNKNOWN,
                               D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS}),
                           TextureFlags::UnorderedAccess)),
          gradients(context, TextureDefinition::TextureDefinition(
                                 Format::R16G16B16A16_Float, N, M, 0,
                                 TextureFlags::UnorderedAccess)) {
      TextureRef &tr = mixMaxDisplacementMap.getTexture();

      _allocatedSubscription = tr->Allocated.subscribe([&](Resource *res) {
        for (u32 i = 0; i < 4; ++i) {
          D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
          uavDesc.Format =
              DXGI_FORMAT(mixMaxDisplacementMap.Definition()->PixelFormat);
          uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
          uavDesc.Texture2D.MipSlice = 0 + i;
          DisplacementMapMipsUAV[i] =
              context.CommonDescriptorHeap
                  ->CreateDescriptor<UnorderedAccessView>(res->get(), &uavDesc);
        }
      });
    }
  };

  LODDataBuffers HighestBuffer;
  LODDataBuffers MediumBuffer;
  LODDataBuffers LowestBuffer;
  const std::array<LODDataBuffers *const, 3> LODs = {
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
                                         Format::R32G32_Float, inp.N, inp.M, 0u,
                                         CalculateTildeh0<f32>(inp)))),
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
};

struct MutableGpuSources {
  struct LODDataSource {
    MutableTexture Foam;
    LODDataSource(ResourceAllocationContext &context,
                  const SimulationData &simData)
        : Foam(MutableTexture(context,
                              TextureDefinition::TextureDefinition(
                                  Format::R32G32_Float, simData.N, simData.M, 0,
                                  TextureFlags::UnorderedAccess))) {}
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
  RootSignature<SimulationStage::FoamDecayDescription> foamDecayRootDescription;

  PipelineState spektrumPipeline;
  PipelineState FFTPipeline;
  PipelineState displacementPipeline;
  PipelineState gradientPipeline;
  PipelineState foamDecayPipeline;
  ConeMapCreater2 coneMapCreater;
  MixMaxCompute mixMaxCompute;

  static FullPipeline Create(GraphicsDevice &device,
                             PipelineStateProvider &pipelineStateProvider);
};
void WaterSimulationComputeShader(
    SimulationStage::SimulationResources &simResource,
    SimulationStage::ConstantGpuSources<Axodox::Graphics::D3D12::MutableTexture>
        &simulationConstantSources,
    SimulationStage::MutableGpuSources &simulationMutableSources,
    const SimulationData &simData,
    SimulationStage::FullPipeline &fullSimPipeline,
    Axodox::Graphics::D3D12::CommandAllocator &computeAllocator,
    Axodox::Graphics::D3D12::GpuVirtualAddress timeDataBuffer, const u32 &N,
    const DebugValues &debugValues,
    const std::array<bool, 3> useLod = {true, true, true});

} // namespace SimulationStage
