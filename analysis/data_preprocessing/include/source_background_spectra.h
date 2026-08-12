#ifndef CSHINE_GAMMA_SOURCE_BACKGROUND_SPECTRA_H
#define CSHINE_GAMMA_SOURCE_BACKGROUND_SPECTRA_H

// Provenance: DP-S000,
// DataPreprocessing/step0-nobkg/20240308-ThnatCo60-ALLOR_NoBkg.C.

#include <string>
#include <vector>

namespace cshine_gamma {

struct RunSampleDefinition {
  std::string prefix;
  int first_file;
  int last_file;
  double live_time_seconds;
};

struct SourceBackgroundDefinition {
  std::string name;
  std::string tree_name;
  RunSampleDefinition source;
  RunSampleDefinition background;
  unsigned int channel_count;
  int adc_bin_count;
  double adc_min;
  double adc_max;
  double time_min_exclusive;
  double time_max_exclusive;
};

struct ChannelSpectrumSummary {
  unsigned int channel;
  long long source_selected_entries;
  long long background_selected_entries;
  double source_all_bins;
  double background_all_bins;
  double net_rate_all_bins;
};

struct SourceBackgroundSummary {
  long long source_tree_entries;
  long long background_tree_entries;
  std::vector<std::string> source_files;
  std::vector<std::string> background_files;
  std::vector<ChannelSpectrumSummary> channels;
};

SourceBackgroundDefinition Central0308SourceBackgroundDefinition();

void ValidateSourceBackgroundDefinition(
    const SourceBackgroundDefinition& definition);

std::string FormatInputFilename(const std::string& input_directory,
                                const std::string& sample_prefix,
                                int file_index);

std::vector<std::string> BuildInputFileList(
    const std::string& input_directory,
    const RunSampleDefinition& sample);

std::string HighGainEnergyBranchName(unsigned int channel);
std::string TimeBranchName(unsigned int channel);
std::string TimeSelection(const SourceBackgroundDefinition& definition,
                          unsigned int channel);
std::string SourceHistogramName(unsigned int channel);
std::string BackgroundHistogramName(unsigned int channel);
std::string NetHistogramName(unsigned int channel);

SourceBackgroundSummary BuildSourceBackgroundSpectra(
    const SourceBackgroundDefinition& definition,
    const std::string& input_directory,
    const std::string& output_root_file,
    const std::string& report_file,
    bool overwrite = false);

void WriteSourceBackgroundReport(
    const std::string& report_file,
    const SourceBackgroundDefinition& definition,
    const SourceBackgroundSummary& summary,
    const std::string& output_root_file,
    bool overwrite = false);

}  // namespace cshine_gamma

#endif  // CSHINE_GAMMA_SOURCE_BACKGROUND_SPECTRA_H
