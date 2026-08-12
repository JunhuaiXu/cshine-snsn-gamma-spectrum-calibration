#include "time_calibration.h"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace cshine_gamma {
namespace {

const std::array<TimeWalkParameter, 15> kCentralParameters = {{
    {19190.0, -381.198, 592.276},
    {12542.3, -218.541, 588.291},
    {16874.5, -443.026, 577.473},
    {18759.2, -607.210, 592.099},
    {17320.0, -495.872, 592.945},
    {15157.8, -258.436, 591.224},
    {10046.1, -193.933, 575.274},
    {13821.7, -400.981, 584.002},
    {11020.5, -291.473, 573.071},
    {13329.8, -310.908, 584.707},
    {10879.0, -235.120, 586.769},
    {9897.46, -295.675, 582.070},
    {7312.49, -67.4275, 583.091},
    {9322.57, -127.872, 586.164},
    {14622.8, -272.735, 594.726},
}};

const TimeWalkParameter& Parameter(unsigned int crystal) {
  if (crystal >= kCentralParameters.size()) {
    throw std::out_of_range("time-calibration crystal index must be 0--14");
  }
  return kCentralParameters[crystal];
}

double WalkTermNs(const TimeWalkParameter& parameter,
                  unsigned short low_gain_channel) {
  const double denominator =
      parameter.e0_channel - static_cast<double>(low_gain_channel);
  if (denominator == 0.0) {
    throw std::domain_error("time-walk denominator is zero");
  }
  return parameter.c0_ns_channel / denominator;
}

}  // namespace

const std::array<TimeWalkParameter, 15>& CentralTimeWalkParameters() {
  return kCentralParameters;
}

double GammaTdcUnitNs() {
  return 8.9 / 0x1E;
}

bool IsValidGammaTdc(unsigned short tdc_channel) {
  return tdc_channel > 100 && tdc_channel < 4000;
}

double DiagnosticGammaTimeNs(unsigned int crystal,
                             unsigned short tdc_channel,
                             unsigned short low_gain_channel) {
  const TimeWalkParameter& parameter = Parameter(crystal);
  return static_cast<double>(tdc_channel) * GammaTdcUnitNs() -
         WalkTermNs(parameter, low_gain_channel);
}

double HistoricalDiagnosticMacroGammaTimeNs(
    unsigned int crystal,
    double already_scaled_gamma_time_ns,
    unsigned short low_gain_channel) {
  if (!(already_scaled_gamma_time_ns >= 0.0) ||
      already_scaled_gamma_time_ns >
          static_cast<double>(std::numeric_limits<unsigned short>::max())) {
    throw std::out_of_range(
        "historical diagnostic scaled time cannot convert to unsigned short");
  }
  return DiagnosticGammaTimeNs(
      crystal, static_cast<unsigned short>(already_scaled_gamma_time_ns),
      low_gain_channel);
}

double CorrectedGammaTimeNs(unsigned int crystal,
                            unsigned short tdc_channel,
                            unsigned short low_gain_channel) {
  const TimeWalkParameter& parameter = Parameter(crystal);
  return DiagnosticGammaTimeNs(crystal, tdc_channel, low_gain_channel) -
         parameter.t0_ns;
}

double CorrectedGammaTimeOrNaNNs(unsigned int crystal,
                                 unsigned short tdc_channel,
                                 unsigned short low_gain_channel) {
  if (!IsValidGammaTdc(tdc_channel)) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  return CorrectedGammaTimeNs(crystal, tdc_channel, low_gain_channel);
}

}  // namespace cshine_gamma
