#pragma once
#include "pch.h"
#include <complex>
#include <numbers>
#include "Defaults.h"
#include <random>

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

  static const auto rowMajorIndexing = [&M](const u32 i, const u32 j) {
    return i * M + j;
  };

  std::vector<std::complex<Prec>> res(N * M);
  for (i32 i = -(i32)N >> 1; i < (i32)N >> 1; i++) {
    for (i32 j = -(i32)M >> 1; j < (i32)M >> 1; j++) {
      const float2 k = float2(2 * std::numbers::pi_v<Prec> * i / L_x,
                              2 * std::numbers::pi_v<Prec> * j / L_z);
      res[rowMajorIndexing(i, j)] =
          tilde_h0<Prec>(k, dis(gen), dis(gen), Amplitude,
                         WindForce * WindForce / gravity, wind);
    }
  }
  return res;
}
