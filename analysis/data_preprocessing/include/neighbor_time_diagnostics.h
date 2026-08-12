#ifndef CSHINE_GAMMA_NEIGHBOR_TIME_DIAGNOSTICS_H
#define CSHINE_GAMMA_NEIGHBOR_TIME_DIAGNOSTICS_H

#include <string>
#include <vector>

namespace cshine_gamma {

struct NeighborTimeDefinition {
  std::string name;
  std::string input_tree_name;
  unsigned int first_crystal;
  unsigned int second_crystal;
  double energy_threshold_mev;
  unsigned int time_bins;
  double time_min_ns;
  double time_max_ns;
  unsigned int difference_bins;
  double difference_min_ns;
  double difference_max_ns;
};

struct NeighborTimeSummary {
  std::vector<std::string> input_specs;
  std::vector<int> files_added_per_spec;
  std::vector<std::string> resolved_input_files;
  unsigned long long tree_entries;
  long long time_correlation_all_rows;
  long long time_correlation_cut_rows;
  long long time_difference_all_rows;
  long long time_difference_cut_rows;
  long long first_time_rows;
  long long second_time_rows;
  double difference_all_peak;
  double difference_cut_peak;
  double historical_peak_scale;
};

NeighborTimeDefinition HistoricalCsI05CsI06NeighborTimeDefinition();

void ValidateNeighborTimeDefinition(const NeighborTimeDefinition& definition);

NeighborTimeSummary BuildNeighborTimeDiagnostics(
    const NeighborTimeDefinition& definition,
    const std::vector<std::string>& input_files_or_patterns,
    const std::string& output_root_file,
    const std::string& report_file,
    bool overwrite = false);

}  // namespace cshine_gamma

#endif
