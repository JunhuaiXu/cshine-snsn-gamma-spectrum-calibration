#include "energy_calibration.h"

#include "t_2d_fit.h"
#include "t_gamma_cali.h"

#include <TCanvas.h>
#include <TFile.h>
#include <TGraphErrors.h>
#include <TH1D.h>
#include <TROOT.h>
#include <TSystem.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

const std::array<double, 15> kSyntheticIntercepts = {{
    -0.156522, -0.0717239, -0.153483, -0.0800695, -0.181024,
    -0.0261985, -0.10527, -0.0171669, -0.140669, -0.0544363,
    -0.0372925, -0.0524932, -0.171461, -0.0694357, -0.132527}};

const std::array<double, 15> kSyntheticSlopes = {{
    0.00289592, 0.00300974, 0.00296201, 0.00279182, 0.00305379,
    0.00305366, 0.00311652, 0.00296461, 0.00297893, 0.00298759,
    0.00286612, 0.00297196, 0.00285588, 0.00303534, 0.00297552}};

bool NearlyEqual(double left, double right, double tolerance) {
  return std::abs(left - right) <= tolerance;
}

bool Check(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAILED: " << message << '\n';
  }
  return condition;
}

double Gaussian(double x, double mean, double sigma, double amplitude) {
  const double displacement = (x - mean) / sigma;
  return amplitude * std::exp(-0.5 * displacement * displacement);
}

void WriteSyntheticSourceSpectra(const std::string& path) {
  TFile output(path.c_str(), "RECREATE");
  if (output.IsZombie()) {
    throw std::runtime_error("cannot create synthetic source spectra");
  }
  const std::array<double, 3> energies = {{1.173, 1.332, 2.614}};
  for (unsigned int channel = 0; channel < 15; ++channel) {
    TH1D histogram(cshine_gamma::CalibrationSourceHistogramName(channel).c_str(),
                   "",
                   4096,
                   0.0,
                   4096.0);
    const double mean_0 =
        (energies[0] - kSyntheticIntercepts[channel]) /
        kSyntheticSlopes[channel];
    const double mean_1 =
        (energies[1] - kSyntheticIntercepts[channel]) /
        kSyntheticSlopes[channel];
    const double mean_2 =
        (energies[2] - kSyntheticIntercepts[channel]) /
        kSyntheticSlopes[channel];
    for (int bin = 1; bin <= histogram.GetNbinsX(); ++bin) {
      const double x = histogram.GetBinCenter(bin);
      const double value =
          0.25 + 0.00005 * x + Gaussian(x, mean_0, 8.0, 9.0) +
          Gaussian(x, mean_1, 9.0, 8.0) + Gaussian(x, mean_2, 12.0, 9.0);
      histogram.SetBinContent(bin, value);
      // This is a deterministic schema-and-fit test rather than a counting
      // experiment.  A uniform small uncertainty keeps all channel-specific
      // historical windows numerically stable without changing the fitted
      // peak positions or widths that the test is designed to exercise.
      histogram.SetBinError(bin, 0.05);
    }
    histogram.Write();
  }
  output.Close();
}

void WriteSyntheticGainRelation(const std::string& path) {
  TFile output(path.c_str(), "RECREATE");
  if (output.IsZombie()) {
    throw std::runtime_error("cannot create synthetic gain relation");
  }
  t_2d_fit gain("f_data", "synthetic_gain.root");
  for (unsigned int channel = 0; channel < 15; ++channel) {
    gain.SetPoint(channel, 60.0 + channel, 0.1, 0.1, 0.001);
  }
  output.WriteTObject(&gain);
  output.Close();
}

std::string ReadTextFile(const std::string& path) {
  std::ifstream input(path.c_str());
  std::ostringstream content;
  content << input.rdbuf();
  return content.str();
}

}  // namespace

int main() {
  gROOT->SetBatch(kTRUE);
  bool ok = true;

  const cshine_gamma::EnergyCalibrationDefinition central =
      cshine_gamma::Central0308EnergyCalibrationDefinition();
  ok &= Check(central.channel_count == 15,
              "central 15-channel calibration definition");
  ok &= Check(central.reference_energies_mev[0] == 1.173 &&
                  central.reference_energies_mev[1] == 1.332 &&
                  central.reference_energies_mev[2] == 2.614,
              "historical three reference energies");
  ok &= Check(central.cobalt_fit_windows[0][0] == 430.0 &&
                  central.cobalt_fit_windows[14][3] == 520.0 &&
                  central.thorium_fit_windows[12][0] == 930.0,
              "historical channel-specific fit windows");
  ok &= Check(cshine_gamma::HistoricalCalibrationCanvasPad(0) == 16 &&
                  cshine_gamma::HistoricalCalibrationCanvasPad(12) == 1 &&
                  cshine_gamma::HistoricalCalibrationCanvasPad(14) == 2,
              "historical 4-by-4 crystal layout");

  std::ostringstream directory_name;
  directory_name << gSystem->TempDirectory() << "/cshine_gamma_energy_cali_"
                 << gSystem->GetPid();
  const std::string directory = directory_name.str();
  gSystem->mkdir(directory.c_str(), true);
  const std::string source_path = directory + "/source.root";
  const std::string gain_path = directory + "/gain.root";
  const std::string output_directory = directory + "/nested-results/m4";
  const std::string output_path = output_directory + "/calibration.root";
  const std::string report_path = output_directory + "/calibration.run.tsv";
  WriteSyntheticSourceSpectra(source_path);
  WriteSyntheticGainRelation(gain_path);

  const cshine_gamma::EnergyCalibrationSummary summary =
      cshine_gamma::FitEnergyCalibration(central,
                                         source_path,
                                         gain_path,
                                         output_path,
                                         report_path,
                                         std::string(),
                                         std::string(),
                                         std::string(),
                                         std::string(),
                                         std::string(),
                                         std::string(),
                                         false);
  ok &= Check(!gSystem->AccessPathName(output_directory.c_str(), kFileExists),
              "nested output directory is created automatically");
  ok &= Check(summary.channels.size() == 15,
              "all 15 channels are calibrated");
  for (unsigned int channel = 0; channel < 15; ++channel) {
    const cshine_gamma::EnergyCalibrationChannelSummary& fit =
        summary.channels[channel];
    ok &= Check(fit.channel == channel &&
                    fit.canvas_pad ==
                        cshine_gamma::HistoricalCalibrationCanvasPad(channel),
                "channel identity and historical canvas pad");
    ok &= Check(fit.cobalt_fit_status == 0 && fit.thorium_fit_status == 0,
                "ROOT source-peak fit status");
    ok &= Check(fit.cobalt_peak_1_width > 5.0 &&
                    fit.cobalt_peak_1_width < 30.0 &&
                    fit.cobalt_peak_2_width > 5.0 &&
                    fit.cobalt_peak_2_width < 30.0 &&
                    fit.thorium_peak_width > 0.0,
                "stored point errors are fitted Gaussian widths");
    ok &= Check(NearlyEqual(fit.intercept_mev,
                            kSyntheticIntercepts[channel],
                            0.03) &&
                    NearlyEqual(fit.slope_mev_per_channel,
                                kSyntheticSlopes[channel],
                                5.0e-5),
                "synthetic three-point calibration recovery");
  }

  TFile result(output_path.c_str(), "READ");
  t_gamma_cali* calibration = nullptr;
  TCanvas* cobalt_canvas = nullptr;
  TCanvas* thorium_canvas = nullptr;
  TCanvas* calibration_canvas = nullptr;
  result.GetObject("cali_20240308", calibration);
  result.GetObject("c_1d_fits_co", cobalt_canvas);
  result.GetObject("c_1d_fits_th", thorium_canvas);
  result.GetObject("c_cali", calibration_canvas);
  ok &= Check(calibration != nullptr && cobalt_canvas != nullptr &&
                  thorium_canvas != nullptr && calibration_canvas != nullptr,
              "historical ROOT calibration object and canvas names");
  if (calibration != nullptr) {
    ok &= Check(std::string(calibration->GetTitle()) == "20240308",
                "historical calibration object title");
    ok &= Check(NearlyEqual(calibration->f_2d_fit[4][0][0], 64.0, 0.0) &&
                    NearlyEqual(calibration->f_2d_fit[4][1][0], 0.1, 0.0),
                "gain relation is embedded in the calibration object");
    ok &= Check(NearlyEqual(calibration->f_gamma_cali_data[4][1][0],
                            summary.channels[4].cobalt_peak_1_width,
                            1.0e-12) &&
                    NearlyEqual(calibration->f_gamma_cali_data[4][1][2],
                                summary.channels[4].thorium_peak_width,
                                1.0e-12),
                "Gaussian widths occupy the historical x-error array");
  }
  TGraphErrors* graph = nullptr;
  result.GetObject("g_4", graph);
  ok &= Check(graph != nullptr && graph->GetN() == 3,
              "three-point graph is stored for each channel");
  if (graph != nullptr) {
    ok &= Check(NearlyEqual(graph->GetErrorX(0),
                            summary.channels[4].cobalt_peak_1_width,
                            1.0e-12) &&
                    NearlyEqual(graph->GetErrorX(2),
                                summary.channels[4].thorium_peak_width,
                                1.0e-12),
                "TGraphErrors retains historical peak-width x errors");
  }
  ok &= Check(result.GetListOfKeys()->GetEntries() == 34,
              "ROOT output contains 30 per-channel fits and four shared objects");
  result.Close();

  const std::string report_text = ReadTextFile(report_path);
  ok &= Check(report_text.find(
                  "config\tpoint_x_error_semantics\tgaussian_peak_width_sigma") !=
                  std::string::npos &&
                  report_text.find("input\tsource_spectra\t") !=
                      std::string::npos &&
                  report_text.find("input\tgain_relation\t") !=
                      std::string::npos &&
                  report_text.find("channel\t0\t16\t0\t0") !=
                      std::string::npos,
              "run report records inputs error semantics layout and fit status");

  bool overwrite_rejected = false;
  try {
    cshine_gamma::FitEnergyCalibration(central,
                                       source_path,
                                       gain_path,
                                       output_path,
                                       report_path,
                                       std::string(),
                                       std::string(),
                                       std::string(),
                                       std::string(),
                                       std::string(),
                                       std::string(),
                                       false);
  } catch (const std::runtime_error&) {
    overwrite_rejected = true;
  }
  ok &= Check(overwrite_rejected, "existing outputs rejected by default");

  gSystem->Unlink(source_path.c_str());
  gSystem->Unlink(gain_path.c_str());
  gSystem->Unlink(output_path.c_str());
  gSystem->Unlink(report_path.c_str());
  std::remove(output_directory.c_str());
  std::remove((directory + "/nested-results").c_str());
  std::remove(directory.c_str());

  return ok ? 0 : 1;
}
