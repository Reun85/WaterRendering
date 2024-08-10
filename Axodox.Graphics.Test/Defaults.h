#pragma once
#include "pch.h"
#include <numbers>
#include <pch.h>
#include <winrt/Windows.UI.Core.h>

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
  float frac_part = value - int_part;
  float back = frac_part * b;
  float diff = back > 0 ? back : -back;
  return diff < epsilon;
}

struct Defaults {
public:
  struct Cam {
    QUALIFIER float3 camStartPos = float3(-1, 10, 0);
    QUALIFIER bool startFirstPerson = true;
  };
  struct App {
    CONST_QUALIFIER float patchSize = 20.0f;
    QUALIFIER float oceanSize = 1000.f;

    static_assert(divides_within_eps(oceanSize, patchSize, 0.001f),
                  "Ocean size has to be multiple of patch size");

    QUALIFIER XMFLOAT4 clearColor = {37.f / 255.f, 37.f / 255.f, 37.f / 255.f,
                                     0};
  };
  struct ComputeShader {
    /// Have to change in gradients.hlsl as well
    CONST_QUALIFIER u32 heightMapDimensions = 1024;
    CONST_QUALIFIER u32 computeShaderGroupsDim1 = 16;
    CONST_QUALIFIER u32 computeShaderGroupsDim2 = 16;
  };
  struct QuadTree {
    QUALIFIER float distanceThreshold = 3e+0f;
    QUALIFIER u32 allocation = 20000;
    QUALIFIER u32 maxDepth = 15;
    QUALIFIER u32 minDepth = 0;
  };
  struct Simulation {
  public:
    CONST_QUALIFIER u32 N = ComputeShader::heightMapDimensions;
    /// Have to change in gradients.hlsl as well
    CONST_QUALIFIER f32 L = App::patchSize;
    QUALIFIER f32 Depth = 100;

    QUALIFIER f32 gravity = 9.81f;

    QUALIFIER f32 WindForce = 1 * 10;
    QUALIFIER float2 WindDirection = float2(-0.4f, -0.9f);

    // A constant that scales the waves
    QUALIFIER f32 Amplitude = 0.45e-3f;

    QUALIFIER f32 timeStep = 1.0f / 60.0f;
    static_assert(isPowerOfTwo(N));
  };
};