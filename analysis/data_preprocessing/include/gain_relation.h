#ifndef CSHINE_GAMMA_GAIN_RELATION_H
#define CSHINE_GAMMA_GAIN_RELATION_H

// Provenance: DP-S100,
// DataPreprocessing/step1-2D/20240308_SnSn_GOAL_ALLCOIN.C.

#include <string>
#include <vector>

namespace cshine_gamma {

struct GainRelationDefinition {
  std::string name;
  std::string tree_name;
  std::string input_prefix;
  int first_file;
  int last_file;
  unsigned int channel_count;
  double low_gain_min_exclusive;
  double low_gain_max_exclusive;
  double fit_high_gain_min;
  double fit_high_gain_max;
  double function_high_gain_min;
  double function_high_gain_max;
  double slope_min;
  double slope_max;
  std::string output_object_name;
  std::string output_object_title;
  std::string canvas_name;
  std::string canvas_title;
  int canvas_width;
  int canvas_height;
  int canvas_columns;
  int canvas_rows;
};

struct GainChannelFitSummary {
  unsigned int channel;
  int canvas_pad;
  long long selected_points;
  long long fit_range_points;
  int fit_status;
  double chi_square;
  int degrees_of_freedom;
  double intercept;
  double intercept_error;
  double slope;
  double slope_error;
};

struct GainRelationSummary {
  long long tree_entries;
  std::vector<std::string> input_files;
  std::vector<GainChannelFitSummary> channels;
};

GainRelationDefinition Central0308GainRelationDefinition();

void ValidateGainRelationDefinition(const GainRelationDefinition& definition);

std::string FormatGainInputFilename(const std::string& input_directory,
                                    const std::string& input_prefix,
                                    int file_index);

std::vector<std::string> BuildGainInputFileList(
    const std::string& input_directory,
    const GainRelationDefinition& definition);

std::string LowGainBranchName(unsigned int channel);
std::string GainHighGainBranchName(unsigned int channel);
std::string GainSelection(const GainRelationDefinition& definition,
                          unsigned int channel);
int HistoricalGainCanvasPad(unsigned int channel);

GainRelationSummary FitGainRelation(
    const GainRelationDefinition& definition,
    const std::string& input_directory,
    const std::string& output_root_file,
    const std::string& parameter_file,
    const std::string& report_file,
    const std::string& canvas_pdf_file = std::string(),
    const std::string& canvas_png_file = std::string(),
    bool overwrite = false);

void WriteGainParameterFile(const std::string& parameter_file,
                            const GainRelationSummary& summary,
                            bool overwrite = false);

void WriteGainRelationReport(const std::string& report_file,
                             const GainRelationDefinition& definition,
                             const GainRelationSummary& summary,
                             const std::string& output_root_file,
                             const std::string& parameter_file,
                             const std::string& canvas_pdf_file,
                             const std::string& canvas_png_file,
                             bool overwrite = false);

}  // namespace cshine_gamma

#endif  // CSHINE_GAMMA_GAIN_RELATION_H
