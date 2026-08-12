#include "t_2d_fit.h"
#include "t_gamma_cali.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <string>

static_assert(sizeof(((t_2d_fit*)nullptr)->f_2d_fit) ==
                  15U * 2U * 2U * sizeof(Double_t),
              "t_2d_fit stored-array layout changed");
static_assert(sizeof(((t_gamma_cali*)nullptr)->f_gamma_cali_data) ==
                  15U * 2U * 3U * sizeof(Double_t),
              "t_gamma_cali calibration-point layout changed");
static_assert(sizeof(((t_gamma_cali*)nullptr)->f_gamma_cali_par) ==
                  15U * 2U * 2U * sizeof(Double_t),
              "t_gamma_cali fit-parameter layout changed");

namespace {

bool NearlyEqual(double left, double right, double tolerance = 1.0e-12) {
  return std::abs(left - right) <= tolerance;
}

bool Check(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAILED: " << message << '\n';
  }
  return condition;
}

void SetCalibrationParameters(t_gamma_cali& calibration) {
  for (std::size_t crystal = 0; crystal < 15; ++crystal) {
    calibration.SetPoint(static_cast<UShort_t>(crystal),
                         0.0,
                         0.0,
                         0.1,
                         0.0);
    calibration.f_gamma_cali_par[crystal][0][0] = 0.0;
    calibration.f_gamma_cali_par[crystal][0][1] = 0.01;
    calibration.f_gamma_cali_par[crystal][1][0] = 0.0;
    calibration.f_gamma_cali_par[crystal][1][1] = 0.0;
  }
}

}  // namespace

int main() {
  bool ok = true;

  t_2d_fit gain_relation("gain_relation", "test gain relation");
  gain_relation.SetPoint(0, 10.0, 0.1, 0.1, 0.001);
  ok &= Check(NearlyEqual(gain_relation.GetXE(0, 210.0), 2000.0),
              "low-to-high gain conversion");
  ok &= Check(NearlyEqual(gain_relation.GetE(0, 2000.0), 210.0),
              "high-to-low gain conversion");
  ok &= Check(NearlyEqual(gain_relation.GetXE(15, 210.0), 0.0),
              "invalid crystal index behavior");

  t_gamma_cali calibration("calibration", "test calibration");
  SetCalibrationParameters(calibration);

  ok &= Check(NearlyEqual(calibration.GetEnergy(0, 100.0, 900.0), 9.0),
              "high-gain branch below the transition");
  ok &= Check(NearlyEqual(calibration.GetEnergy(0, 320.0, 3000.0), 30.8),
              "historical weighted transition branch");
  ok &= Check(NearlyEqual(calibration.GetEnergy(0, 360.0, 3000.0), 36.0),
              "low-gain branch above the transition");
  ok &= Check(NearlyEqual(calibration.GetEnergy(0, 100.0, 4001.0), 10.0),
              "high-gain saturation branch");

  std::array<UShort_t, 32> adc{};
  adc[0] = 100;
  adc[16] = 900;
  const auto energies = calibration.GetEnergy(adc);
  ok &= Check(NearlyEqual(energies[0], 9.0),
              "15-crystal array conversion interface");

  std::array<Double_t, 32> floating_adc{};
  floating_adc[0] = 100.9;
  floating_adc[16] = 900.9;
  const auto truncated_energies = calibration.GetEnergy(floating_adc);
  ok &= Check(NearlyEqual(truncated_energies[0], 9.0),
              "historical unsigned-short ADC conversion semantics");

  t_gamma_cali copy(calibration);
  ok &= Check(NearlyEqual(copy.GetEnergy(0, 320.0, 3000.0), 30.8),
              "calibration copy semantics");

  return ok ? 0 : 1;
}
