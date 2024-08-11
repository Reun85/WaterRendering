#pragma once
#include "pch.h"
#include <numbers>
#include <pch.h>
#include <winrt/Windows.UI.Core.h>

#include "constants.h"

using namespace std;
using namespace winrt;

using namespace Windows;
using namespace Windows::Foundation::Numerics;

using namespace DirectX;
using namespace DirectX::PackedVector;

// These variables are only used on the CPU side.
#define QUALIFIER static const constexpr
// These values are also defined somewhere in the shaders, meaning changing
// these values require checking shader files as well.
#define CONST_QUALIFIER static const constexpr

constexpr bool isPowerOfTwo(u32 x) { return std::has_single_bit(x); }

constexpr bool divides_within_eps(float a, float b, float epsilon) noexcept {
  float value = a / b;

  int int_part = static_cast<int>(value);
  float frac_part = value - static_cast<float>(int_part);
  float back = frac_part * b;
  float diff = back > 0 ? back : -back;
  return diff < epsilon;
}

struct Defaults {
public:
  struct Cam {
    QUALIFIER float3 camStartPos = float3(-23, 10, 11);
    QUALIFIER bool startFirstPerson = true;
  };

  struct App {
    CONST_QUALIFIER u16 maxInstances = ShaderConstantCompat::numInstances;
    QUALIFIER f32 oceanSize = 10000.f;

    QUALIFIER XMFLOAT4 clearColor = {37.f / 255.f, 37.f / 255.f, 37.f / 255.f,
                                     0};
  };
  struct ComputeShader {
    /// Have to change in gradients.hlsl as well
    CONST_QUALIFIER u32 heightMapDimensions = ShaderConstantCompat::dispMapSize;
    CONST_QUALIFIER u32 computeShaderGroupsDim1 = 16;
    CONST_QUALIFIER u32 computeShaderGroupsDim2 = 16;
  };
  struct QuadTree {
    QUALIFIER f32 distanceThreshold = 2e+2f;
    QUALIFIER u32 allocation = 20000;
    QUALIFIER u32 maxDepth = 20;
    QUALIFIER u32 minDepth = 0;
  };
  struct Simulation {
    struct KnownToWork {
      struct Large {
        CONST_QUALIFIER f32 patchSize = 400.0f;
        QUALIFIER f32 WindForce = 30 * 6.f;
        QUALIFIER float2 WindDirection = float2(-0.4f, -0.9f);
        QUALIFIER f32 Amplitude = 0.45e-6f;
      };
      struct Small {
        CONST_QUALIFIER f32 patchSize = 40.0f;
        QUALIFIER f32 WindForce = 6.f;
        QUALIFIER float2 WindDirection = float2(-0.4f, -0.9f);
        QUALIFIER f32 Amplitude = 0.45e-3f;
      };
    };

    /// Have to change in common.hlsli as well
    CONST_QUALIFIER f32 patchSize = ShaderConstantCompat::patchSize;
    static_assert(divides_within_eps(App::oceanSize, patchSize, 0.001f),
                  "Ocean size has to be multiple of patch size");
    CONST_QUALIFIER u32 N = ComputeShader::heightMapDimensions;
    QUALIFIER f32 Depth = 100;

    QUALIFIER f32 gravity = 9.81f;

    QUALIFIER f32 WindForce = 6.f;
    QUALIFIER float2 WindDirection = float2(-0.4f, -0.9f);

    // A constant that scales the waves
    QUALIFIER f32 Amplitude = 0.45e-6f;

    QUALIFIER f32 timeStep = 1.0f / 60.0f;
    static_assert(isPowerOfTwo(N));
  };
};
#undef QUALIFIER
#undef CONST_QUALIFIER