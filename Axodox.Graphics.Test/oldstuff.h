#include "pch.h"
using namespace Axodox::Infrastructure;
using namespace Axodox::Storage;
using namespace DirectX::PackedVector;

struct oldWaterPixelShaderData {

  float3 SurfaceColor = float3(0.109, 0.340, 0.589);
  float Roughness = 0.192;
  float3 _TipColor = float3(231.f, 231.f, 231.f) / 255.f;
  float foamDepthFalloff = 0.245;
  // float3 _ScatterColor = float3(4.f / 255.f, 22.f / 255.f, 33.f / 255.f);
  // float3 _ScatterColor = float3(10.f, 18.f, 29.f) / 255.f;
  // float3 _ScatterColor = float3(5.f, 18.f, 34.f) / 255.f;
  float3 _ScatterColor = float3(0.f, 11.f, 25.f) / 255.f;
  float foamRoughnessModifier = 5.0f;
  float NormalDepthAttenuation = 1;
  float _HeightModifier = 60.f;
  float _WavePeakScatterStrength = 4.629f;
  float _ScatterStrength = 0.78f;
  float _ScatterShadowStrength = 4.460f;
};
