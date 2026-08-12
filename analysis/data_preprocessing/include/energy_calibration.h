#ifndef CSHINE_GAMMA_ENERGY_CALIBRATION_H
#define CSHINE_GAMMA_ENERGY_CALIBRATION_H

// Provenance: DP-S200 and DP-S201,
// DataPreprocessing/step2-fit/20240308_ThnatCo60_NoBkg.C and rootlogon.C.

#include <array>
#include <string>
#include <vector>

namespace cshine_gamma {

struct EnergyCalibrationDefinition {
  std::string name;
  unsigned int channel_count;
  std::string source_histogram_prefix;
  std::string gain_relation_object_name;
  std::string calibration_object_name;
  std::string calibration_object_title;
  std::array<double, 3> reference_energies_mev;
  std::array<std::array<double, 4>, 15> cobalt_fit_windows;
  std::array<std::array<double, 2>, 15> thorium_fit_windows;
  double fit_function_min;
  double fit_function_max;
  std::string cobalt_canvas_name;
  std::string cobalt_canvas_title;
  std::string thorium_canvas_name;
  std::string thorium_canvas_title;
  std::string calibration_canvas_name;
  std::string calibration_canvas_title;
  int fit_canvas_width;
  int fit_canvas_height;
  int calibration_canvas_width;
  int calibration_canvas_height;
  int canvas_columns;
  int canvas_rows;
};

struct EnergyCalibrationChannelSummary {
  unsigned int channel;
  int canvas_pad;
  int cobalt_fit_status;
  int thorium_fit_status;
  double cobalt_peak_1_channel;
  double cobalt_peak_1_width;
  double cobalt_peak_2_channel;
  double cobalt_peak_2_width;
  double thorium_peak_channel;
  double thorium_peak_width;
  double intercept_mev;
  double intercept_error_mev;
  double slope_mev_per_channel;
  double slope_error_mev_per_channel;
  double calibration_chi_square;
  int calibration_degrees_of_freedom;
};

struct EnergyCalibrationSummary {
  std::string source_spectra_file;
  std::string gain_relation_file;
  std::vector<EnergyCalibrationChannelSummary> channels;
};

EnergyCalibrationDefinition Central0308EnergyCalibrationDefinition();

void ValidateEnergyCalibrationDefinition(
    const EnergyCalibrationDefinition& definition);

std::string CalibrationSourceHistogramName(unsigned int channel);
int HistoricalCalibrationCanvasPad(unsigned int channel);

EnergyCalibrationSummary FitEnergyCalibration(
    const EnergyCalibrationDefinition& definition,
    const std::string& source_spectra_file,
    const std::string& gain_relation_file,
    const std::string& output_root_file,
    const std::string& report_file,
    const std::string& cobalt_canvas_pdf_file = std::string(),
    const std::string& cobalt_canvas_png_file = std::string(),
    const std::string& thorium_canvas_pdf_file = std::string(),
    const std::string& thorium_canvas_png_file = std::string(),
    const std::string& calibration_canvas_pdf_file = std::string(),
    const std::string& calibration_canvas_png_file = std::string(),
    bool overwrite = false);

void WriteEnergyCalibrationReport(
    const std::string& report_file,
    const EnergyCalibrationDefinition& definition,
    const EnergyCalibrationSummary& summary,
    const std::string& output_root_file,
    const std::string& cobalt_canvas_pdf_file,
    const std::string& cobalt_canvas_png_file,
    const std::string& thorium_canvas_pdf_file,
    const std::string& thorium_canvas_png_file,
    const std::string& calibration_canvas_pdf_file,
    const std::string& calibration_canvas_png_file,
    bool overwrite = false);

}  // namespace cshine_gamma

#endif  // CSHINE_GAMMA_ENERGY_CALIBRATION_H
