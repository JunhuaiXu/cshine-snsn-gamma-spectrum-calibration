#ifndef CSHINE_GAMMA_OBSERVED_SPECTRUM_H
#define CSHINE_GAMMA_OBSERVED_SPECTRUM_H

// M11 migration of the final histogram arithmetic in EnergySpecGen.C.
// Slow coincidence scales the beam-off spectrum in 110--200 MeV.  The
// equal-width fast/random-window cross-check uses a fixed scale of one.

#include <string>

namespace cshine_gamma {

enum class BackgroundSubtractionMode {
  kSlowBeamOff,
  kFastRandomWindow
};

struct ObservedSpectrumDefinition {
  std::string name;
  std::string input_histogram_name;
  std::string output_histogram_name;
  int input_bins;
  double energy_min_mev;
  double energy_max_mev;
  int rebin_factor;
  double normalization_min_mev;
  double normalization_max_mev;
};

struct ObservedSpectrumSummary {
  std::string mode;
  double signal_normalization_counts;
  double background_normalization_counts;
  double background_scale;
  int output_bins;
  double output_energy_min_mev;
  double output_energy_max_mev;
};

// M12 freezes the detector-level spectrum passed to later analyses.  It does
// not select a fit range, transform the energy frame, or rebin for a consumer.
struct ObservedSpectrumInterfaceDefinition {
  std::string object_name;
  std::string object_class;
  int bins;
  double energy_min_mev;
  double energy_max_mev;
  double bin_width_mev;
  std::string energy_frame;
  std::string value_semantics;
  std::string uncertainty_semantics;
};

struct ObservedSpectrumInterfaceSummary {
  std::string object_class;
  int bins;
  double energy_min_mev;
  double energy_max_mev;
  double bin_width_mev;
  double regular_bin_sum;
  double regular_variance_sum;
  double underflow;
  double underflow_error;
  double overflow;
  double overflow_error;
  int negative_regular_bins;
};

ObservedSpectrumDefinition HistoricalObservedSpectrumDefinition();

ObservedSpectrumInterfaceDefinition
HistoricalObservedSpectrumInterfaceDefinition();

void ValidateObservedSpectrumDefinition(
    const ObservedSpectrumDefinition& definition);

ObservedSpectrumSummary BuildObservedSpectrum(
    const ObservedSpectrumDefinition& definition,
    const std::string& signal_root_file,
    const std::string& background_root_file,
    const std::string& output_root_file,
    const std::string& report_file,
    BackgroundSubtractionMode mode,
    bool overwrite = false);

ObservedSpectrumInterfaceSummary InspectObservedSpectrumInterface(
    const ObservedSpectrumInterfaceDefinition& definition,
    const std::string& input_root_file,
    const std::string& report_file,
    bool overwrite = false);

}  // namespace cshine_gamma

#endif  // CSHINE_GAMMA_OBSERVED_SPECTRUM_H
