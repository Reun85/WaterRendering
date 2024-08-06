#pragma once
#include "pch.h"
#include <complex>
#include <numbers>
#include "Defaults.h"
#include <random>

namespace Inner {
template <typename Prec = f32>
  requires std::is_floating_point_v<Prec>
constexpr Prec PhilipsSpektrum(const float2 &k, const Prec &Amplitude,
                               const Prec &largestHeight, const float2 &wind) {

  // Smaller than these waves
  const float l = largestHeight / 1000.f;

  const float kdotw = dot(k, wind);
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
constexpr u32 Indexing(const u32 i, const u32 j, const u32 N, const u32 M) {
  // return ColumnMajorIndexing(i, j, N);
  return RowMajorIndexing(i, j, M);
};
} // namespace Inner

template <typename Prec = float>
  requires std::is_floating_point_v<Prec>
std::vector<std::complex<Prec>>
CalculateTildeh0FromDefaults(std::random_device &rd, const u32 _N,
                             const u32 _M) {

  const auto N = (i32)_N;
  const auto M = (i32)_M;
  using Def = Defaults::Simulation;
  const auto &wind = normalize(Def::WindDirection);
  const auto &gravity = Def::gravity;
  const auto &WindForce = Def::WindForce;
  const auto &Amplitude = Def::Amplitude;
  const auto &L_x = Def::L_x;
  const auto &L_z = Def::L_z;

  std::mt19937 gen(rd());
  std::normal_distribution<Prec> dis(0, 1);

  const i32 Nx2 = N / 2;
  const i32 Mx2 = M / 2;
  std::vector<std::complex<Prec>> res(N * M);
  float2 k(0, 0);
  for (i32 i = 0; i < N; ++i) {
    k.x = 2 * std::numbers::pi_v<Prec> * (Nx2 - i) / L_x;
    for (i32 j = 0; j < M; j++) {
      k.y = 2 * std::numbers::pi_v<Prec> * (Mx2 - j) / L_z;

      const auto index = Inner::Indexing(i, j, N, M);

      res[index] = Inner::tilde_h0<Prec>(k, dis(gen), dis(gen), Amplitude,
                                         WindForce * WindForce / gravity, wind);
    }
  }

  return res;
}

template <typename Prec = float>
  requires std::is_floating_point_v<Prec>
constexpr std::vector<Prec> CalculateFrequenciesFromDefaults(const u32 _N,
                                                             const u32 _M) {

  // w^2(k) = gktanh(kD)
  using Def = Defaults::Simulation;
  const auto &gravity = Def::gravity;
  const auto &D = Def::Depth;
  const auto &L_x = Def::L_x;
  const auto &L_z = Def::L_z;

  const i32 N = (i32)_N;
  const i32 M = (i32)_M;
  const i32 Nx2 = N / 2;
  const i32 Mx2 = M / 2;

  std::vector<Prec> res(N * M);
  float2 kvec(0, 0);
  for (i32 i = 0; i < N; ++i) {
    kvec.x = 2 * std::numbers::pi_v<Prec> * (Nx2 - i) / L_x;
    for (i32 j = 0; j < M; j++) {
      kvec.y = 2 * std::numbers::pi_v<Prec> * (Mx2 - j) / L_z;

      const auto index = Inner::Indexing(i, j, N, M);

      const float k = length(kvec);
      const float tmp = gravity * k * std::tanh(k * D);
      res[index] = sqrtf(tmp);
    }
  }
  return res;
}
