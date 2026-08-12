#ifndef CSHINE_GAMMA_CALIBRATED_EVENT_TREE_H
#define CSHINE_GAMMA_CALIBRATED_EVENT_TREE_H

// Provenance: DP-S500--DP-S503,
// DataPreprocessing/step4-convert.0308.PreRun/{Makefile,rootlogon.C,
// gamma_data.hh,aa_example.C}.  M6 extracts only event-level calibrated
// energy/time and retained raw arrays, including the 32-slot hardware trigger
// monitor TDC array used by M10B.  M8 extends the retained M6 boundary
// with the three historical veto ADC/TDC pairs required to reconstruct
// count_veto; shower reconstruction itself remains a separate stage.

#include <array>
#include <string>
#include <vector>

namespace cshine_gamma {

struct CalibratedEventDefinition {
  std::string name;
  std::string input_tree_name;
  std::string output_tree_name;
  std::string output_tree_title;
  std::string calibration_object_name;
  unsigned int crystal_count;
  unsigned int adc_storage_count;
  unsigned int tdc_storage_count;
  unsigned int t0_count;
  unsigned int veto_count;
};

struct CalibratedEventChannelSummary {
  unsigned long long invalid_time_count;
  unsigned long long high_gain_count;
  unsigned long long blended_gain_count;
  unsigned long long low_gain_count;
  unsigned long long saturated_high_gain_count;
};

struct CalibratedEventSummary {
  std::vector<std::string> input_specs;
  std::vector<int> files_added_per_spec;
  std::vector<std::string> resolved_input_files;
  unsigned long long input_entries;
  unsigned long long output_entries;
  std::array<CalibratedEventChannelSummary, 15> channels;
};

CalibratedEventDefinition Central0308CalibratedEventDefinition();

void ValidateCalibratedEventDefinition(
    const CalibratedEventDefinition& definition);

CalibratedEventSummary BuildCalibratedEventTree(
    const CalibratedEventDefinition& definition,
    const std::vector<std::string>& input_files_or_patterns,
    const std::string& calibration_root_file,
    const std::string& output_root_file,
    const std::string& report_file,
    bool overwrite = false);

}  // namespace cshine_gamma

#endif  // CSHINE_GAMMA_CALIBRATED_EVENT_TREE_H
