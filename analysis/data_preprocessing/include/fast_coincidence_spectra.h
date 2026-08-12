#ifndef CSHINE_GAMMA_FAST_COINCIDENCE_SPECTRA_H
#define CSHINE_GAMMA_FAST_COINCIDENCE_SPECTRA_H

// M11 fast-coincidence cross-check migrated from the frozen RemoveSSDM2
// data/background branches.  It consumes the stored M8 reconstruction and
// writes equal-width signal- and random-window spectra for direct subtraction.

#include <string>
#include <vector>

namespace cshine_gamma {

struct FastCoincidenceDefinition {
  std::string name;
  std::string input_tree_name;
  unsigned int trigger_index;
  unsigned short trigger_min_exclusive;
  unsigned short trigger_max_exclusive;
  double signal_time_min_ns;
  double signal_time_max_ns;
  double random_time_min_ns;
  double random_time_max_ns;
  int energy_bins;
  double energy_min_mev;
  double energy_max_mev;
};

struct FastCoincidenceSummary {
  std::vector<std::string> input_specs;
  std::vector<int> files_added_per_spec;
  std::vector<std::string> resolved_input_files;
  unsigned long long input_entries;
  unsigned long long excluded_ssd_m2_entries;
  unsigned long long signal_central_candidates;
  unsigned long long signal_side_candidates;
  unsigned long long random_central_candidates;
  unsigned long long random_side_candidates;
};

FastCoincidenceDefinition HistoricalFastCoincidenceDefinition();

void ValidateFastCoincidenceDefinition(
    const FastCoincidenceDefinition& definition);

FastCoincidenceSummary BuildFastCoincidenceSpectra(
    const FastCoincidenceDefinition& definition,
    const std::vector<std::string>& input_files_or_patterns,
    const std::string& signal_output_root_file,
    const std::string& random_output_root_file,
    const std::string& report_file,
    bool overwrite = false);

}  // namespace cshine_gamma

#endif  // CSHINE_GAMMA_FAST_COINCIDENCE_SPECTRA_H
