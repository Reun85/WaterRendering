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

#define QUALIFIER static const constexpr

// Checks
constexpr bool isPowerOfTwo(u32 x) { return std::has_single_bit(x); }

struct Defaults {

public:
  struct Cam {
    QUALIFIER float3 camStartPos = float3(-1, 300, 0);
    QUALIFIER bool startFirstPerson = true;
  };
  struct App {
    QUALIFIER float planeSize = 100.0f;
    QUALIFIER XMFLOAT4 clearColor = {0, 1, 0, 0};
  };
  struct ComputeShader {
    QUALIFIER u32 heightMapDimensionsX = 1024;
    QUALIFIER u32 heightMapDimensionsZ = 1024;
    QUALIFIER u32 computeShaderGroupsDim1 = 16;
    QUALIFIER u32 computeShaderGroupsDim2 = 16;
  };
  struct QuadTree {
    QUALIFIER float distanceThreshold = 5e+1f;
    QUALIFIER u32 allocation = 10000;
    QUALIFIER u32 maxDepth = 5;
    QUALIFIER u32 minDepth = 0;
  };
  struct Simulation {
  public:
    QUALIFIER u32 N = ComputeShader::heightMapDimensionsX;
    QUALIFIER u32 M = ComputeShader::heightMapDimensionsZ;
    QUALIFIER f32 L_x = App::planeSize;
    QUALIFIER f32 L_z = App::planeSize;
    // QUALIFIER f32 L_x = 1;
    // QUALIFIER f32 L_z = 1;
    QUALIFIER f32 Depth = 100;

    QUALIFIER f32 gravity = 9.81f;

    QUALIFIER f32 WindForce = 6.5;
    QUALIFIER float2 WindDirection = float2(-0.4f, -0.9f);

    // A constant that scales the waves
    // QUALIFIER f32 Amplitude = 0.45e-5f;
    QUALIFIER f32 Amplitude = 0.45e-3f;

    QUALIFIER f32 timeStep = 1.0f / 60.0f;
    static_assert(isPowerOfTwo(N));
    static_assert(isPowerOfTwo(M));
  };
};