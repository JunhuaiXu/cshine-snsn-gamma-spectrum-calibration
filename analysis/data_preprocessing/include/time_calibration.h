#ifndef CSHINE_GAMMA_TIME_CALIBRATION_H
#define CSHINE_GAMMA_TIME_CALIBRATION_H

// Provenance: DP-S400 and DP-S403,
// DataPreprocessing/step3-time/gamma_time_cali.hh and
// DataPreprocessing/step3-time/timeFigs/gamma_time_cali.hh.
//
// This interface preserves the published parameter application.  It does not
// claim to reproduce the missing historical batch fit that created f_00.root
// through f_14.root and the corresponding text outputs.

#include <array>
#include <cstddef>

namespace cshine_gamma {

struct TimeWalkParameter {
  double c0_ns_channel;
  double e0_channel;
  double t0_ns;
};

const std::array<TimeWalkParameter, 15>& CentralTimeWalkParameters();

double GammaTdcUnitNs();
bool IsValidGammaTdc(unsigned short tdc_channel);

double DiagnosticGammaTimeNs(unsigned int crystal,
                             unsigned short tdc_channel,
                             unsigned short low_gain_channel);

// Reproduce the implicit double-to-unsigned conversion in the historical
// timeFigs/time_cali.C diagnostic macro.  Do not use this function to produce
// the event-level GammaTime array.
double HistoricalDiagnosticMacroGammaTimeNs(
    unsigned int crystal,
    double already_scaled_gamma_time_ns,
    unsigned short low_gain_channel);

double CorrectedGammaTimeNs(unsigned int crystal,
                            unsigned short tdc_channel,
                            unsigned short low_gain_channel);

double CorrectedGammaTimeOrNaNNs(unsigned int crystal,
                                 unsigned short tdc_channel,
                                 unsigned short low_gain_channel);

template <class TdcArray, class AdcArray>
std::array<double, 15> CorrectedGammaTimeArrayNs(
    const TdcArray& tdc_channels,
    const AdcArray& low_gain_channels) {
  std::array<double, 15> result;
  for (std::size_t crystal = 0; crystal < result.size(); ++crystal) {
    result[crystal] = CorrectedGammaTimeOrNaNNs(
        static_cast<unsigned int>(crystal),
        tdc_channels[crystal],
        low_gain_channels[crystal]);
  }
  return result;
}

}  // namespace cshine_gamma

#endif  // CSHINE_GAMMA_TIME_CALIBRATION_H
