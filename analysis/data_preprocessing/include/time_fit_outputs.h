#ifndef CSHINE_GAMMA_TIME_FIT_OUTPUTS_H
#define CSHINE_GAMMA_TIME_FIT_OUTPUTS_H

#include "time_calibration.h"

#include <array>
#include <string>
#include <vector>

namespace cshine_gamma {

struct TimeFitTextSummary {
  double chi2;
  int ndf;
  double edm;
  int calls;
  TimeWalkParameter parameter;
  std::array<double, 3> parameter_errors;
};

struct TimeFitArtifactSummary {
  unsigned int crystal;
  std::string root_file;
  std::string text_file;
  std::string histogram_name;
  std::string fit_function_name;
  TimeFitTextSummary fit;
  bool parameters_match_production;
};

TimeFitTextSummary ReadTimeFitTextSummary(const std::string& path);

std::vector<TimeFitArtifactSummary> InspectTimeFitArtifacts(
    const std::string& fits_directory,
    double parameter_tolerance = 1.0e-9);

void WriteTimeFitArtifactReport(
    const std::string& path,
    const std::vector<TimeFitArtifactSummary>& summaries,
    bool overwrite = false);

}  // namespace cshine_gamma

#endif  // CSHINE_GAMMA_TIME_FIT_OUTPUTS_H
