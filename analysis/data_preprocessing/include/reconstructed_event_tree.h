#ifndef CSHINE_GAMMA_RECONSTRUCTED_EVENT_TREE_H
#define CSHINE_GAMMA_RECONSTRUCTED_EVENT_TREE_H

// Portable M8 producer derived directly from DP-S600.  It consumes an M6
// GammaCaliData tree, retains the M6 hardware trigger-monitor array, appends
// the historical recon_result and count_veto
// branches, and writes the three per-crystal reconstructed-energy histogram
// families consumed by M9.

#include <string>
#include <vector>

namespace cshine_gamma {

struct ReconstructedEventDefinition {
  std::string name;
  std::string input_tree_name;
  std::string output_tree_name;
  std::string output_tree_title;
};

struct ReconstructedEventSummary {
  std::vector<std::string> input_specs;
  std::vector<int> files_added_per_spec;
  std::vector<std::string> resolved_input_files;
  unsigned long long input_entries;
  unsigned long long output_entries;
  unsigned long long entries_without_reconstruction;
  unsigned long long valid_reconstruction_count;
  unsigned long long placeholder_reconstruction_count;
  unsigned long long main_spectrum_candidate_count;
  unsigned long long side_candidate_rejected_by_veto_count;
  unsigned long long events_by_veto_signal_count[4];
};

ReconstructedEventDefinition HistoricalReconstructedEventDefinition();

void ValidateReconstructedEventDefinition(
    const ReconstructedEventDefinition& definition);

ReconstructedEventSummary BuildReconstructedEventTree(
    const ReconstructedEventDefinition& definition,
    const std::vector<std::string>& input_files_or_patterns,
    const std::string& output_root_file,
    const std::string& report_file,
    bool overwrite = false);

}  // namespace cshine_gamma

#endif  // CSHINE_GAMMA_RECONSTRUCTED_EVENT_TREE_H
