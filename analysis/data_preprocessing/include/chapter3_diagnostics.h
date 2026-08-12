#ifndef CSHINE_GAMMA_CHAPTER3_DIAGNOSTICS_H
#define CSHINE_GAMMA_CHAPTER3_DIAGNOSTICS_H

// M10A producer for the reviewed Chapter 3 beam-on/beam-off diagnostics.
// It consumes the recon_result branch written by M8; shower reconstruction is
// deliberately not repeated here.

#include <string>
#include <vector>

namespace cshine_gamma {

struct Chapter3DiagnosticsDefinition {
  std::string name;
  std::string input_tree_name;
  double crystal_pitch_cm;
  double high_energy_threshold_mev;
};

struct Chapter3DiagnosticsSummary {
  std::string sample_role;
  std::vector<std::string> input_specs;
  std::vector<int> files_added_per_spec;
  std::vector<std::string> resolved_input_files;
  unsigned long long input_entries;
  unsigned long long valid_reconstruction_count;
  unsigned long long placeholder_reconstruction_count;
  unsigned long long main_candidate_count;
  unsigned long long central_candidate_count;
  unsigned long long side_candidate_count;
  unsigned long long side_candidate_rejected_by_veto_count;
  unsigned long long all15_candidate_count;
  unsigned long long triggers_with_reconstruction;
  unsigned long long triggers_with_high_energy_reconstruction;
};

Chapter3DiagnosticsDefinition HistoricalChapter3DiagnosticsDefinition();

void ValidateChapter3DiagnosticsDefinition(
    const Chapter3DiagnosticsDefinition& definition);

Chapter3DiagnosticsSummary BuildChapter3Diagnostics(
    const Chapter3DiagnosticsDefinition& definition,
    const std::vector<std::string>& input_files_or_patterns,
    const std::string& output_root_file,
    const std::string& report_file,
    const std::string& sample_role,
    bool overwrite = false);

}  // namespace cshine_gamma

#endif  // CSHINE_GAMMA_CHAPTER3_DIAGNOSTICS_H
