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

  const double klength = length(k);
  const double val1 = Amplitude / pow(klength, 4);
  const double val2 =
      std::exp(-1 / (klength * klength * largestHeight * largestHeight));
  const double tmp = abs(dot(normalize(k), normalize(wind)));
  const double val3 = tmp * tmp;

  return static_cast<Prec>(val1 * val2 * val3);
}

template <typename Prec = f32>
  requires std::is_floating_point_v<Prec>
constexpr std::complex<Prec>
tilde_h0(const float2 &k, const Prec &xi_real, const Prec &xi_im,
         const Prec &Amplitude, const Prec &largestHeight, const float2 &wind) {
  std::complex res = std::complex<Prec>(xi_real, xi_im);
  res *= sqrt(PhilipsSpektrum<Prec>(k, Amplitude, largestHeight, wind));
  res /= std::numbers::sqrt2_v<Prec>;
  return res;
}

constexpr u32 RowMajorIndexing(const u32 i, const u32 j, const u32 M) {
  return i * M + j;
};
constexpr u32 ColumnMajorIndexing(const u32 i, const u32 j, const u32 N) {
  return i + j * N;
};
constexpr u32 Indexing(const u32 i, const u32 j, const u32 N, const u32 M) {
  return ColumnMajorIndexing(i, j, N);
  // return RowMajorIndexing(i, j, M);
};
} // namespace Inner

template <typename Prec = float>
  requires std::is_floating_point_v<Prec>
std::vector<std::complex<Prec>>
CalculateTildeh0FromDefaults(std::random_device &rd, const u32 N, const u32 M) {

  using Def = Defaults::Simulation;
  const auto &wind = Def::WindDirection;
  const auto &gravity = Def::gravity;
  const auto &WindForce = Def::WindForce;
  const auto &Amplitude = Def::Amplitude;
  const auto &L_x = Def::L_x;
  const auto &L_z = Def::L_z;

  std::mt19937 gen(rd());
  std::uniform_real_distribution<Prec> dis(0, 1);

  std::vector<std::complex<Prec>> res(N * M);
  for (u32 i = 0; i < N; i++) {
    for (u32 j = 0; j < M; j++) {
      const float2 k = float2(
          2 * std::numbers::pi_v<Prec> * ((i32)i - (((i32)N) >> 1)) / L_x,
          2 * std::numbers::pi_v<Prec> * ((i32)j - (((i32)M) >> 1)) / L_z);
      res[Inner::Indexing(i, j, N, M)] =
          Inner::tilde_h0<Prec>(k, dis(gen), dis(gen), Amplitude,
                                WindForce * WindForce / gravity, wind);
    }
  }
  return res;
}

template <typename Prec = float>
  requires std::is_floating_point_v<Prec>
constexpr std::vector<Prec> CalculateFrequenciesFromDefaults(const u32 N,
                                                             const u32 M) {

  // w^2(k) = gktanh(kD)
  using Def = Defaults::Simulation;
  const auto &gravity = Def::gravity;
  const auto &D = Def::Depth;
  const auto &L_x = Def::L_x;
  const auto &L_z = Def::L_z;

  std::vector<Prec> res(N * M);
  for (u32 i = 0; i < N; i++) {
    for (u32 j = 0; j < M; j++) {
      const float2 kvec = float2(
          2 * std::numbers::pi_v<Prec> * ((i32)i - (((i32)N) >> 1)) / L_x,
          2 * std::numbers::pi_v<Prec> * ((i32)j - (((i32)M) >> 1)) / L_z);
      const float k = length(kvec);
      res[Inner::Indexing(i, j, N, M)] = gravity * k * std::tanh(k * D);
    }
  }
  return res;
}
