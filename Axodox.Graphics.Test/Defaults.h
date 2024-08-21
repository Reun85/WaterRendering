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
    QUALIFIER f32 oceanSize = 1000.f;

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
    QUALIFIER f32 quadTreeDistanceThreshold = 2e+2f;
    QUALIFIER u32 allocation = 20000;
    QUALIFIER u32 maxDepth = 40;
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
    QUALIFIER f32 patchSize1 = 7.f;
    QUALIFIER f32 patchSize2 = 27.f;
    QUALIFIER f32 patchSize3 = 87.f;
    CONST_QUALIFIER u32 N = ComputeShader::heightMapDimensions;

    QUALIFIER float3 displacementLambda = float3(0.78, 0.6, 0.78);
    QUALIFIER f32 exponentialDecay = 0.3f;

    QUALIFIER f32 Depth = 100;

    QUALIFIER f32 gravity = 9.81f;

    QUALIFIER float2 WindDirection = float2(-0.4f, -0.9f);

    // A constant that scales the waves
    QUALIFIER f32 Amplitude1 = 0.45e-1f;
    QUALIFIER f32 Amplitude2 = 0.45e-2f;
    QUALIFIER f32 Amplitude3 = 0.45e-3f;

    QUALIFIER f32 WindForce1 = 3.f;
    QUALIFIER f32 WindForce2 = 4.f;
    QUALIFIER f32 WindForce3 = 8.f;

    static_assert(isPowerOfTwo(N));
  };
};
#undef QUALIFIER
#undef CONST_QUALIFIER