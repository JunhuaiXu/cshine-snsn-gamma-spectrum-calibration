#include "time_fit_outputs.h"

#include "output_path_support.h"

#include <TCanvas.h>
#include <TFile.h>
#include <TF1.h>
#include <TH2.h>
#include <TList.h>
#include <TObject.h>
#include <TPad.h>
#include <TSystem.h>

#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace cshine_gamma {
namespace {

std::string JoinPath(const std::string& directory, const std::string& name) {
  if (directory.empty() || directory == ".") {
    return name;
  }
  if (directory[directory.size() - 1] == '/') {
    return directory + name;
  }
  return directory + "/" + name;
}

std::string FitBaseName(unsigned int crystal) {
  std::ostringstream name;
  name << "f_" << std::setfill('0') << std::setw(2) << crystal;
  return name.str();
}

double ParseNumberAfterEquals(const std::string& line,
                              const std::string& field) {
  const std::string::size_type equals = line.find('=');
  if (equals == std::string::npos) {
    throw std::runtime_error("missing '=' for field " + field);
  }
  std::istringstream value_stream(line.substr(equals + 1));
  double value = std::numeric_limits<double>::quiet_NaN();
  if (!(value_stream >> value)) {
    throw std::runtime_error("cannot parse field " + field);
  }
  return value;
}

double ParseErrorAfterPlusMinus(const std::string& line,
                                const std::string& field) {
  const std::string marker = "+/-";
  const std::string::size_type position = line.find(marker);
  if (position == std::string::npos) {
    throw std::runtime_error("missing '+/-' for field " + field);
  }
  std::istringstream error_stream(line.substr(position + marker.size()));
  double error = std::numeric_limits<double>::quiet_NaN();
  if (!(error_stream >> error)) {
    throw std::runtime_error("cannot parse uncertainty for field " + field);
  }
  return error;
}

bool BeginsWithField(const std::string& line, const std::string& field) {
  if (line.size() < field.size() || line.compare(0, field.size(), field) != 0) {
    return false;
  }
  return line.size() == field.size() || line[field.size()] == ' ' ||
         line[field.size()] == '\t';
}

TH2* FindFirstHistogram(TPad* pad) {
  if (pad == 0 || pad->GetListOfPrimitives() == 0) {
    return 0;
  }
  TIter next(pad->GetListOfPrimitives());
  while (TObject* object = next()) {
    if (object->InheritsFrom(TH2::Class())) {
      return static_cast<TH2*>(object);
    }
    if (object->InheritsFrom(TPad::Class())) {
      TH2* nested = FindFirstHistogram(static_cast<TPad*>(object));
      if (nested != 0) {
        return nested;
      }
    }
  }
  return 0;
}

std::string FirstFitFunctionName(TH2* histogram) {
  if (histogram == 0 || histogram->GetListOfFunctions() == 0) {
    return std::string();
  }
  TIter next(histogram->GetListOfFunctions());
  while (TObject* object = next()) {
    if (object->InheritsFrom(TF1::Class())) {
      return object->GetName();
    }
  }
  return std::string();
}

bool NearlyEqual(double left, double right, double tolerance) {
  return std::abs(left - right) <= tolerance;
}

}  // namespace

TimeFitTextSummary ReadTimeFitTextSummary(const std::string& path) {
  std::ifstream input(path.c_str());
  if (!input) {
    throw std::runtime_error("cannot open time-fit text output: " + path);
  }

  TimeFitTextSummary summary;
  summary.chi2 = std::numeric_limits<double>::quiet_NaN();
  summary.ndf = -1;
  summary.edm = std::numeric_limits<double>::quiet_NaN();
  summary.calls = -1;
  summary.parameter = {std::numeric_limits<double>::quiet_NaN(),
                       std::numeric_limits<double>::quiet_NaN(),
                       std::numeric_limits<double>::quiet_NaN()};
  summary.parameter_errors.fill(std::numeric_limits<double>::quiet_NaN());

  std::string line;
  while (std::getline(input, line)) {
    if (BeginsWithField(line, "Chi2")) {
      summary.chi2 = ParseNumberAfterEquals(line, "Chi2");
    } else if (BeginsWithField(line, "NDf")) {
      summary.ndf = static_cast<int>(ParseNumberAfterEquals(line, "NDf"));
    } else if (BeginsWithField(line, "Edm")) {
      summary.edm = ParseNumberAfterEquals(line, "Edm");
    } else if (BeginsWithField(line, "NCalls")) {
      summary.calls =
          static_cast<int>(ParseNumberAfterEquals(line, "NCalls"));
    } else if (BeginsWithField(line, "C0")) {
      summary.parameter.c0_ns_channel = ParseNumberAfterEquals(line, "C0");
      summary.parameter_errors[0] = ParseErrorAfterPlusMinus(line, "C0");
    } else if (BeginsWithField(line, "E0")) {
      summary.parameter.e0_channel = ParseNumberAfterEquals(line, "E0");
      summary.parameter_errors[1] = ParseErrorAfterPlusMinus(line, "E0");
    } else if (BeginsWithField(line, "T0")) {
      summary.parameter.t0_ns = ParseNumberAfterEquals(line, "T0");
      summary.parameter_errors[2] = ParseErrorAfterPlusMinus(line, "T0");
    }
  }

  if (!std::isfinite(summary.chi2) || summary.ndf < 0 ||
      !std::isfinite(summary.edm) || summary.calls < 0 ||
      !std::isfinite(summary.parameter.c0_ns_channel) ||
      !std::isfinite(summary.parameter.e0_channel) ||
      !std::isfinite(summary.parameter.t0_ns) ||
      !std::isfinite(summary.parameter_errors[0]) ||
      !std::isfinite(summary.parameter_errors[1]) ||
      !std::isfinite(summary.parameter_errors[2])) {
    throw std::runtime_error("incomplete time-fit text output: " + path);
  }
  return summary;
}

std::vector<TimeFitArtifactSummary> InspectTimeFitArtifacts(
    const std::string& fits_directory,
    double parameter_tolerance) {
  if (parameter_tolerance < 0.0) {
    throw std::invalid_argument("parameter tolerance must be non-negative");
  }

  const std::array<TimeWalkParameter, 15>& production =
      CentralTimeWalkParameters();
  std::vector<TimeFitArtifactSummary> summaries;
  summaries.reserve(production.size());

  for (unsigned int crystal = 0; crystal < production.size(); ++crystal) {
    const std::string base = FitBaseName(crystal);
    const std::string root_path = JoinPath(fits_directory, base + ".root");
    const std::string text_path = JoinPath(fits_directory, base + ".out");
    if (gSystem->AccessPathName(root_path.c_str(), kFileExists)) {
      throw std::runtime_error("missing historical time-fit ROOT file: " +
                               root_path);
    }

    TFile root_file(root_path.c_str(), "READ");
    if (root_file.IsZombie()) {
      throw std::runtime_error("cannot open historical time-fit ROOT file: " +
                               root_path);
    }
    TCanvas* canvas = dynamic_cast<TCanvas*>(root_file.Get("Canvas_1"));
    if (canvas == 0) {
      throw std::runtime_error("missing Canvas_1 in " + root_path);
    }
    TH2* histogram = FindFirstHistogram(canvas);
    if (histogram == 0) {
      throw std::runtime_error("Canvas_1 contains no TH2 object in " +
                               root_path);
    }

    TimeFitArtifactSummary artifact;
    artifact.crystal = crystal;
    artifact.root_file = base + ".root";
    artifact.text_file = base + ".out";
    artifact.histogram_name = histogram->GetName();
    artifact.fit_function_name = FirstFitFunctionName(histogram);
    artifact.fit = ReadTimeFitTextSummary(text_path);
    artifact.parameters_match_production =
        NearlyEqual(artifact.fit.parameter.c0_ns_channel,
                    production[crystal].c0_ns_channel,
                    parameter_tolerance) &&
        NearlyEqual(artifact.fit.parameter.e0_channel,
                    production[crystal].e0_channel,
                    parameter_tolerance) &&
        NearlyEqual(artifact.fit.parameter.t0_ns,
                    production[crystal].t0_ns,
                    parameter_tolerance);
    summaries.push_back(artifact);
  }
  return summaries;
}

void WriteTimeFitArtifactReport(
    const std::string& path,
    const std::vector<TimeFitArtifactSummary>& summaries,
    bool overwrite) {
  if (!overwrite && !gSystem->AccessPathName(path.c_str(), kFileExists)) {
    throw std::runtime_error("refusing to overwrite existing report: " + path);
  }
  detail::EnsureOutputParentDirectory(path);
  std::ofstream output(path.c_str());
  if (!output) {
    throw std::runtime_error("cannot create time-fit report: " + path);
  }
  output << "crystal\troot_file\ttext_file\thistogram\tfit_function\tchi2\tndf"
            "\tedm\tncalls\tC0\tC0_error\tE0\tE0_error\tT0\tT0_error"
            "\tparameters_match_production\n";
  output << std::setprecision(12);
  for (std::vector<TimeFitArtifactSummary>::const_iterator item =
           summaries.begin();
       item != summaries.end(); ++item) {
    output << item->crystal << '\t' << item->root_file << '\t'
           << item->text_file << '\t' << item->histogram_name << '\t'
           << (item->fit_function_name.empty() ? "none"
                                               : item->fit_function_name)
           << '\t' << item->fit.chi2 << '\t' << item->fit.ndf << '\t'
           << item->fit.edm << '\t' << item->fit.calls << '\t'
           << item->fit.parameter.c0_ns_channel << '\t'
           << item->fit.parameter_errors[0] << '\t'
           << item->fit.parameter.e0_channel << '\t'
           << item->fit.parameter_errors[1] << '\t'
           << item->fit.parameter.t0_ns << '\t'
           << item->fit.parameter_errors[2] << '\t'
           << (item->parameters_match_production ? "true" : "false") << '\n';
  }
}

}  // namespace cshine_gamma
