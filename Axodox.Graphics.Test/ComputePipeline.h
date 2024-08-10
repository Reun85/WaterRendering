
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
  struct SpektrumRootDescription : public RootSignatureMask {
    // In
    RootDescriptorTable<1> Tildeh0;
    RootDescriptorTable<1> Frequencies;
    struct InputConstants {
      float time;
    };
    RootDescriptor<RootDescriptorType::ConstantBuffer> InputBuffer;
    // Out
    RootDescriptorTable<1> Tildeh;
    RootDescriptorTable<1> TildeD;

    explicit SpektrumRootDescription(const RootSignatureContext &context)
        : RootSignatureMask(context),
          Tildeh0(this, {DescriptorRangeType::ShaderResource, 0}),
          Frequencies(this, {DescriptorRangeType::ShaderResource, 1}),
          InputBuffer(this, {0}),
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

  struct SimulationResources {
    CommandAllocator Allocator;
    CommandFence Fence;
    CommandFenceMarker FrameDoneMarker;
    DynamicBufferManager DynamicBuffer;

    MutableTextureWithState tildeh;
    MutableTextureWithState tildeD;
#define useDifferentFFTOutputBuffers true
#if useDifferentFFTOutputBuffers
    MutableTextureWithState FFTTildeh;
    MutableTextureWithState FFTTildeD;
#else
    MutableTextureWithState &FFTTildeh = tildeh;
    MutableTextureWithState &FFTTildeD = tildeD;
#endif
    MutableTextureWithState tildehBuffer;
    MutableTextureWithState tildeDBuffer;

    MutableTextureWithState finalDisplacementMap;
    MutableTextureWithState gradients;

    explicit SimulationResources(const ResourceAllocationContext &context,
                                 const u32 N, const u32 M)
        : Allocator(*context.Device),

          Fence(*context.Device), DynamicBuffer(*context.Device),
          tildeh(context, TextureDefinition::TextureDefinition(
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
          finalDisplacementMap(context, TextureDefinition::TextureDefinition(
                                            Format::R32G32B32A32_Float, N, M, 0,
                                            TextureFlags::UnorderedAccess)),

          gradients(context, TextureDefinition::TextureDefinition(
                                 Format::R16G16B16A16_Float, N, M, 0,
                                 TextureFlags::UnorderedAccess))

    {
    }
  };
};
