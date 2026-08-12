#ifndef CSHINE_GAMMA_TRIGGER_DIAGNOSTICS_H
#define CSHINE_GAMMA_TRIGGER_DIAGNOSTICS_H

// M10B trigger-monitor and trigger-conditioned diagnostics.  The monitor
// spectra preserve the strict historical TDC window.  The conditioned
// energy--time objects expose both the selection implemented by the frozen
// Fig. 15 macro and the reviewed main-analysis central/side veto definition.

#include <array>
#include <string>
#include <vector>

namespace cshine_gamma {

struct TriggerDiagnosticsDefinition {
  std::string name;
  std::string input_tree_name;
  unsigned short monitor_min_exclusive;
  unsigned short monitor_max_exclusive;
  unsigned short condition_min_inclusive;
  unsigned short condition_max_inclusive;
  std::array<unsigned short, 5> conditioned_indices;
};

struct TriggerDiagnosticsSummary {
  std::vector<std::string> input_specs;
  std::vector<int> files_added_per_spec;
  std::vector<std::string> resolved_input_files;
  unsigned long long input_entries;
  std::array<unsigned long long, 15> monitor_entries;
  std::array<unsigned long long, 5> conditioned_event_entries;
  std::array<unsigned long long, 5> historical_candidate_entries;
  std::array<unsigned long long, 5> reviewed_candidate_entries;
};

TriggerDiagnosticsDefinition HistoricalTriggerDiagnosticsDefinition();

void ValidateTriggerDiagnosticsDefinition(
    const TriggerDiagnosticsDefinition& definition);

TriggerDiagnosticsSummary BuildTriggerDiagnostics(
    const TriggerDiagnosticsDefinition& definition,
    const std::vector<std::string>& input_files_or_patterns,
    const std::string& output_root_file,
    const std::string& report_file,
    bool overwrite = false);

}  // namespace cshine_gamma

#endif  // CSHINE_GAMMA_TRIGGER_DIAGNOSTICS_H
