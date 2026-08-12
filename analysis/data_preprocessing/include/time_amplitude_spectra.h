#ifndef CSHINE_GAMMA_TIME_AMPLITUDE_SPECTRA_H
#define CSHINE_GAMMA_TIME_AMPLITUDE_SPECTRA_H

// Provenance: DP-S405 and DP-S406,
// DataPreprocessing/step3-time/timeFigs/time_orig.C and time_cali.C.
//
// The original and historical-diagnostic modes intentionally remain
// distinct.  The latter preserves the implicit conversion in time_cali.C;
// it is not the production GammaTime definition used by the event analysis.

#include <array>
#include <string>
#include <vector>

namespace cshine_gamma {

enum class TimeAmplitudeMode {
  kOriginal,
  kHistoricalDiagnosticCorrected,
};

struct TimeAmplitudeDefinition {
  std::string name;
  std::string tree_name;
  std::vector<std::string> input_patterns;
  TimeAmplitudeMode mode;
  unsigned int channel_count;
  unsigned int t0_count;
  unsigned int thread_count;
  int time_bin_count;
  double time_min_ns;
  double time_max_ns;
  int adc_bin_count;
  double adc_min_channel;
  double adc_max_channel;
  int tdc_min_inclusive;
  int tdc_max_inclusive;
  double gamma_tdc_unit_ns;
  double t0_tdc_unit_ns;
  std::array<double, 4> t0_offsets_ns;
};

struct TimeAmplitudeSummary {
  std::vector<std::string> input_patterns;
  std::vector<int> files_added_per_pattern;
  long long input_file_count;
  long long tree_entries;
  std::vector<double> average_t0_histogram_entries;
  std::vector<std::vector<double> > individual_t0_histogram_entries;
};

TimeAmplitudeDefinition CentralOriginalTimeAmplitudeDefinition();
TimeAmplitudeDefinition CentralHistoricalCorrectedTimeAmplitudeDefinition();

const char* TimeAmplitudeModeName(TimeAmplitudeMode mode);
void ValidateTimeAmplitudeDefinition(const TimeAmplitudeDefinition& definition);

std::string LowGainEnergyBranchName(unsigned int channel);
std::string GammaTimeBranchName(unsigned int channel);
std::string T0TimeBranchName(unsigned int channel);
std::string IndividualT0HistogramName(unsigned int crystal,
                                      unsigned int t0_channel);
std::string AverageT0HistogramName(TimeAmplitudeMode mode,
                                   unsigned int crystal);

TimeAmplitudeSummary BuildTimeAmplitudeSpectra(
    const TimeAmplitudeDefinition& definition,
    const std::string& input_directory,
    const std::string& output_root_file,
    const std::string& report_file,
    bool overwrite);

}  // namespace cshine_gamma

#endif  // CSHINE_GAMMA_TIME_AMPLITUDE_SPECTRA_H
