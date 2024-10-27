#pragma once
#include "pch.h"
#include <complex>
#include <numbers>
#include "Defaults.h"
#include "QuadTree.h"
#include <random>

struct NeedToDo;

struct SimulationData {
  u32 N;
  u32 M;
  struct PatchData {
    float3 displacementLambda;
    f32 patchSize;
    f32 foamExponentialDecay;
    f32 Amplitude;
    f32 WindForce;

    f32 foamMinValue;
    f32 foamBias;
    f32 foamMult;
    u32 N;
    u32 M;
    float2 windDirection;
    f32 gravity;
    f32 Depth;
    bool DrawImGui(std::string_view ID);
    PatchData &operator=(const PatchData &other) = default;
  };
  float2 windDirection;
  f32 gravity;
  f32 Depth;
  PatchData Highest;
  PatchData Medium;
  PatchData Lowest;
  float quadTreeDistanceThreshold = QuadTree::Defaults::DistanceThreshold;
  SimulationData &operator=(const SimulationData &other) = default;

public:
  void DrawImGui(NeedToDo &out, bool exclusiveWindow = true);
  static SimulationData Default();
  static std::vector<std::pair<std::string, SimulationData>> Presets();
};

namespace Inner {
template <typename Prec = f32>
  requires std::is_floating_point_v<Prec>
constexpr Prec PhilipsSpektrum(const float2 &k, const Prec &Amplitude,
                               const Prec &largestHeight, const float2 &wind) {
  // Smaller than these waves
  const float l = largestHeight / 1000.f;

  const float kdotw = dot(k, normalize(wind));
  const float klengthsq = dot(k, k);
  float P_h = Amplitude *
              (std::expf(-1.0f / (klengthsq * largestHeight * largestHeight))) /
              (klengthsq * klengthsq * klengthsq) * (kdotw * kdotw);

  if (kdotw < 0.0f) {
    // wave is moving against wind direction w
    P_h *= 0.07f;
  }

  return P_h * std::expf(-klengthsq * l * l);
}

template <typename Prec = f32>
  requires std::is_floating_point_v<Prec>
constexpr std::complex<Prec>
tilde_h0(const float2 &k, const Prec &xi_real, const Prec &xi_im,
         const Prec &Amplitude, const Prec &largestHeight, const float2 &wind) {
  constexpr static const Prec one_over_sqrt_2 = 1 / std::numbers::sqrt2_v<Prec>;

  std::complex res = std::complex<Prec>(xi_real, xi_im);

  Prec sqrt_Ph = 0;
  if (k.x != 0.f || k.y != 0.f) {
    sqrt_Ph =
        std::sqrtf(PhilipsSpektrum<Prec>(k, Amplitude, largestHeight, wind));
  }

  res *= sqrt_Ph;
  res *= one_over_sqrt_2;
  return res;
}

constexpr u32 RowMajorIndexing(const u32 i, const u32 j, const u32 M) {
  return i * M + j;
};
constexpr u32 ColumnMajorIndexing(const u32 i, const u32 j, const u32 N) {
  return i + j * N;
};
constexpr u32 Indexing(const u32 i, const u32 j, [[maybe_unused]] const u32 _,
                       const u32 M) {
  // return ColumnMajorIndexing(i, j, N);
  return RowMajorIndexing(i, j, M);
};
} // namespace Inner

class Xorshift128 {
public:
  using result_type = uint32_t;

  explicit Xorshift128(uint32_t seed = std::random_device{}()) {
    // Seed the state with a non-zero seed
    state[0] = seed;
    state[1] = seed ^ 0x6C8E9CF570932BD5ULL;
    state[2] = seed ^ 0xDEADBEEFDEADBEEFULL;
    state[3] = seed ^ 0xBADDCAFEFEEDFACEULL;
  }

  static constexpr result_type min() { return 0; }
  static constexpr result_type max() { return UINT32_MAX; }

  uint32_t operator()() {
    // Xorshift128 algorithm
    uint32_t t = state[3];
    t ^= t << 11;
    t ^= t >> 8;
    state[3] = state[2];
    state[2] = state[1];
    state[1] = state[0];
    t ^= state[0];
    t ^= state[0] >> 19;
    state[0] = t;
    return t;
  }

private:
  std::array<uint32_t, 4> state;
};

template <typename Prec = float>
  requires std::is_floating_point_v<Prec>
std::vector<std::complex<Prec>>
CalculateTildeh0(const SimulationData::PatchData &dat) {
  const auto N = (i32)dat.N;
  const auto M = (i32)dat.M;
  const auto &wind = normalize(dat.windDirection);
  const auto &gravity = dat.gravity;
  const auto &WindForce = dat.WindForce;
  const auto &Amplitude = dat.Amplitude;
  const auto &L = dat.patchSize;

  std::random_device rd;
  Xorshift128 gen(rd());
  std::normal_distribution<Prec> dis(0, 1);

  const i32 Nx2 = N / 2;
  const i32 Mx2 = M / 2;
  std::vector<std::complex<Prec>> res(N * M);
  float2 k(0, 0);
  for (i32 i = 0; i < N; ++i) {
    k.x = 2.f * std::numbers::pi_v<Prec> * static_cast<float>(Nx2 - i) / L;
    for (i32 j = 0; j < M; j++) {
      k.y = 2.f * std::numbers::pi_v<Prec> * static_cast<float>(Mx2 - j) / L;

      const auto index = Inner::Indexing(i, j, N, M);

      res[index] = Inner::tilde_h0<Prec>(k, dis(gen), dis(gen), Amplitude,
                                         WindForce * WindForce / gravity, wind);
    }
  }

  return res;
}

template <typename Prec = float>
  requires std::is_floating_point_v<Prec>
constexpr std::vector<Prec>
CalculateFrequencies(const SimulationData::PatchData &dat) {
  // w^2(k) = gktanh(kD)
  const auto &gravity = dat.gravity;
  const auto &D = dat.Depth;
  const auto &L = dat.patchSize;

  const i32 N = (i32)dat.N;
  const i32 M = (i32)dat.M;
  const i32 Nx2 = N / 2;
  const i32 Mx2 = M / 2;

  std::vector<Prec> res(N * M);
  float2 kvec(0, 0);
  for (i32 i = 0; i < N; ++i) {
    kvec.x = 2 * std::numbers::pi_v<Prec> * (Nx2 - i) / L;
    for (i32 j = 0; j < M; j++) {
      kvec.y = 2 * std::numbers::pi_v<Prec> * (Mx2 - j) / L;

      const auto index = Inner::Indexing(i, j, N, M);

      const float k = length(kvec);
      const float tmp = gravity * k * std::tanh(k * D);
      res[index] = sqrtf(tmp);
    }
  }
  return res;
}
