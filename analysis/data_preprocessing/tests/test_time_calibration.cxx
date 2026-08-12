#include "time_calibration.h"

#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

bool NearlyEqual(double left, double right, double tolerance) {
  return std::abs(left - right) <= tolerance;
}

bool Check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAILED: " << message << '\n';
  }
  return condition;
}

}  // namespace

int main() {
  bool ok = true;

  const std::array<cshine_gamma::TimeWalkParameter, 15>& parameters =
      cshine_gamma::CentralTimeWalkParameters();
  ok &= Check(parameters.size() == 15, "15 parameter triplets");
  ok &= Check(NearlyEqual(parameters[0].c0_ns_channel, 19190.0, 0.0) &&
                  NearlyEqual(parameters[14].e0_channel, -272.735, 0.0) &&
                  NearlyEqual(parameters[5].t0_ns, 591.224, 0.0),
              "historical central parameters");
  ok &= Check(NearlyEqual(cshine_gamma::GammaTdcUnitNs(),
                          8.9 / 30.0,
                          1.0e-15),
              "historical gamma TDC unit");
  ok &= Check(!cshine_gamma::IsValidGammaTdc(100) &&
                  cshine_gamma::IsValidGammaTdc(101) &&
                  cshine_gamma::IsValidGammaTdc(3999) &&
                  !cshine_gamma::IsValidGammaTdc(4000),
              "strict historical TDC validity window");

  const unsigned int crystal = 5;
  const unsigned short tdc = 2000;
  const unsigned short adc = 500;
  const double expected_diagnostic =
      static_cast<double>(tdc) * (8.9 / 30.0) -
      15157.8 / (-258.436 - static_cast<double>(adc));
  const double expected_production = expected_diagnostic - 591.224;
  ok &= Check(NearlyEqual(cshine_gamma::DiagnosticGammaTimeNs(
                              crystal, tdc, adc),
                          expected_diagnostic,
                          1.0e-12),
              "diagnostic header formula without T0 subtraction");
  const double scaled_gamma_time =
      static_cast<double>(tdc) * cshine_gamma::GammaTdcUnitNs();
  const unsigned short truncated_scaled_time =
      static_cast<unsigned short>(scaled_gamma_time);
  const double expected_historical_macro =
      static_cast<double>(truncated_scaled_time) * (8.9 / 30.0) -
      15157.8 / (-258.436 - static_cast<double>(adc));
  ok &= Check(NearlyEqual(
                  cshine_gamma::HistoricalDiagnosticMacroGammaTimeNs(
                      crystal, scaled_gamma_time, adc),
                  expected_historical_macro,
                  1.0e-12),
              "historical time_cali macro implicit conversion");
  ok &= Check(NearlyEqual(cshine_gamma::CorrectedGammaTimeNs(
                              crystal, tdc, adc),
                          expected_production,
                          1.0e-12),
              "production formula with T0 subtraction");
  ok &= Check(NearlyEqual(
                  cshine_gamma::DiagnosticGammaTimeNs(crystal, tdc, adc) -
                      cshine_gamma::CorrectedGammaTimeNs(crystal, tdc, adc),
                  parameters[crystal].t0_ns,
                  1.0e-12),
              "production/diagnostic difference is exactly T0");
  ok &= Check(std::isnan(cshine_gamma::CorrectedGammaTimeOrNaNNs(
                  crystal, 100, adc)) &&
                  std::isnan(cshine_gamma::CorrectedGammaTimeOrNaNNs(
                      crystal, 4000, adc)),
              "invalid TDC channels return NaN");

  std::array<unsigned short, 15> tdc_array;
  std::array<unsigned short, 15> adc_array;
  tdc_array.fill(2000);
  adc_array.fill(500);
  tdc_array[3] = 0;
  const std::array<double, 15> corrected =
      cshine_gamma::CorrectedGammaTimeArrayNs(tdc_array, adc_array);
  ok &= Check(std::isnan(corrected[3]) &&
                  NearlyEqual(corrected[5], expected_production, 1.0e-12),
              "array conversion preserves per-channel validity");

  bool caught = false;
  try {
    cshine_gamma::CorrectedGammaTimeNs(15, 2000, 500);
  } catch (const std::out_of_range&) {
    caught = true;
  }
  ok &= Check(caught, "out-of-range crystal index is rejected");

  return ok ? 0 : 1;
}
