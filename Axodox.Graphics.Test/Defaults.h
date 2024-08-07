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

// Checks
constexpr bool isPowerOfTwo(u32 x) { return std::has_single_bit(x); }

struct Defaults {
public:
  struct Cam {
    QUALIFIER float3 camStartPos = float3(-1, 10, 0);
    QUALIFIER bool startFirstPerson = true;
  };
  struct App {
    /// Have to change in gradients.hlsl as well
    CONST_QUALIFIER float planeSize = 20.0f;
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
    QUALIFIER float distanceThreshold = 1e+1f;
    QUALIFIER u32 allocation = 10000;
    QUALIFIER u32 maxDepth = 7;
    QUALIFIER u32 minDepth = 0;
  };
  struct Simulation {
  public:
    CONST_QUALIFIER u32 N = ComputeShader::heightMapDimensions;
    CONST_QUALIFIER f32 L_x = App::planeSize;
    CONST_QUALIFIER f32 L_z = App::planeSize;
    QUALIFIER f32 Depth = 100;

    QUALIFIER f32 gravity = 9.81f;

    QUALIFIER f32 WindForce = 1 * 10;
    QUALIFIER float2 WindDirection = float2(-0.4f, -0.9f);

    // A constant that scales the waves
    QUALIFIER f32 Amplitude = 0.45e-3f;
    // QUALIFIER f32 Amplitude = 0.45e-3f;

    QUALIFIER f32 timeStep = 1.0f / 60.0f;
    static_assert(isPowerOfTwo(N));
  };
};