#ifndef CSHINE_GAMMA_RECONSTRUCTED_SPECTRUM_MERGE_H
#define CSHINE_GAMMA_RECONSTRUCTED_SPECTRUM_MERGE_H

// Portable M9 replacement for the historical hadd + all_recon.C sequence.
// Input files are explicit M8 per-run outputs.  The first output preserves the
// summed per-crystal histogram families; the second preserves the reviewed
// central, side, and total spectrum objects used downstream.

#include <string>
#include <vector>

namespace cshine_gamma {

struct ReconstructedSpectrumMergeDefinition {
  std::string name;
  unsigned int crystal_count;
  int energy_bins;
  double energy_min_mev;
  double energy_max_mev;
  std::vector<unsigned short> central_crystals;
  std::vector<unsigned short> side_crystals;
};

struct ReconstructedSpectrumMergeSummary {
  std::string sample_role;
  std::vector<std::string> input_files;
  double central_entries;
  double side_veto_silent_entries;
  double total_entries;
};

ReconstructedSpectrumMergeDefinition
HistoricalReconstructedSpectrumMergeDefinition();

void ValidateReconstructedSpectrumMergeDefinition(
    const ReconstructedSpectrumMergeDefinition& definition);

ReconstructedSpectrumMergeSummary MergeReconstructedSpectra(
    const ReconstructedSpectrumMergeDefinition& definition,
    const std::vector<std::string>& input_root_files,
    const std::string& per_crystal_output_root_file,
    const std::string& merged_output_root_file,
    const std::string& report_file,
    const std::string& sample_role,
    bool overwrite = false);

}  // namespace cshine_gamma

#endif  // CSHINE_GAMMA_RECONSTRUCTED_SPECTRUM_MERGE_H
