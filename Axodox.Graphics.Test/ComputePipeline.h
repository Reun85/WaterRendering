#pragma once
#include "pch.h"
#include "Camera.h"
#include "QuadTree.h"
#include <fstream>
#include <string.h>
#include "ConstantGPUBuffer.h"
#include "Defaults.h"
#include "WaterMath.h"
#include "Helpers.h"
#include "pix3.h"

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

struct SimulationStage {
  struct TimeData {
    float deltaTime;
    float timeSinceLaunch;
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
    // Out
    RootDescriptorTable<1> Output;

    explicit DisplacementDescription(const RootSignatureContext &context)
        : RootSignatureMask(context),
          Height(this, {DescriptorRangeType::ShaderResource, 0}),
          Choppy(this, {DescriptorRangeType::ShaderResource, 1}),
          Output(this, {DescriptorRangeType::UnorderedAccess, 0}) {
      Flags = RootSignatureFlags::None;
    }
  };
  struct GradientDescription : public RootSignatureMask {
    // In
    RootDescriptorTable<1> Displacement;
    // Out
    RootDescriptorTable<1> Output;

    explicit GradientDescription(const RootSignatureContext &context)
        : RootSignatureMask(context),
          Displacement(this, {DescriptorRangeType::ShaderResource, 0}),
          Output(this, {DescriptorRangeType::UnorderedAccess, 0}) {
      Flags = RootSignatureFlags::None;
    }
  };
  struct FoamDecayDescription : public RootSignatureMask {
    // In-Out
    RootDescriptorTable<1> Gradients;
    // In
    RootDescriptorTable<1> Foam;
    RootDescriptor<RootDescriptorType::ConstantBuffer> timeBuffer;

    explicit FoamDecayDescription(const RootSignatureContext &context)
        : RootSignatureMask(context),
          Gradients(this, {DescriptorRangeType::UnorderedAccess, 0}),
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
    struct LODData {
      MutableTextureWithState tildeh;
      MutableTextureWithState tildeD;
#if useDifferentFFTOutputBuffers
      MutableTextureWithState FFTTildeh;
      MutableTextureWithState FFTTildeD;
#else
      MutableTextureWithState &FFTTildeh = tildeh;
      MutableTextureWithState &FFTTildeD = tildeD;
#endif
      MutableTextureWithState tildehBuffer;
      MutableTextureWithState tildeDBuffer;
      MutableTextureWithState DisplacementMap;
      MutableTextureWithState gradients;

      LODData(const ResourceAllocationContext &context, const u32 N,
              const u32 M)
          : tildeh(context, TextureDefinition::TextureDefinition(
                                Format::R32G32_Float, N, M, 0,
                                TextureFlags::UnorderedAccess)),
            tildeD(context, TextureDefinition::TextureDefinition(
                                Format::R32G32_Float, N, M, 0,
                                TextureFlags::UnorderedAccess)),
#if useDifferentFFTOutputBuffers
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
            DisplacementMap(context, TextureDefinition::TextureDefinition(
                                         Format::R32G32B32A32_Float, N, M, 0,
                                         TextureFlags::UnorderedAccess)),
            gradients(context, TextureDefinition::TextureDefinition(
                                   Format::R16G16B16A16_Float, N, M, 0,
                                   TextureFlags::UnorderedAccess)) {
      }
    };

    LODData Highest;
    LODData Medium;
    LODData Lowest;
    const std::array<const LODData *const, 3> LODs = {&Highest, &Medium,
                                                      &Lowest};

    explicit SimulationResources(const ResourceAllocationContext &context,
                                 const u32 N, const u32 M)
        : Allocator(*context.Device),

          Fence(*context.Device), DynamicBuffer(*context.Device),
          Highest(context, N, M), Medium(context, N, M), Lowest(context, N, M) {
    }
  };

  struct ConstantGpuSources {
    struct LODData {
      ImmutableTexture Tildeh0;
      ImmutableTexture Frequencies;
    };
    LODData Highest;
    LODData Medium;
    LODData Lowest;
    const std::array<const LODData *const, 3> LODs = {&Highest, &Medium,
                                                      &Lowest};
    // ImmutableTexture PerlinNoise;
  };
  struct GpuSources {
    MutableTexture Foam;
  };
};
