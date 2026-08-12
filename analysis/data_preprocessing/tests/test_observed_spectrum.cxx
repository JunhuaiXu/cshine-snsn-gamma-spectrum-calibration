#include "observed_spectrum.h"

#include <TFile.h>
#include <TH1.h>
#include <TH1I.h>
#include <TSystem.h>

#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void Require(bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}
void WriteInput(const std::string& path,
                double low_energy_counts,
                double normalization_counts) {
  TFile output(path.c_str(), "RECREATE");
  TH1I histogram("h_total_E_M1", "", 1000, 0.0, 200.0);
  histogram.Fill(40.1, low_energy_counts);
  histogram.Fill(120.1, normalization_counts);
  histogram.Write();
}
TH1* ReadDifference(TFile& file) {
  TH1* histogram = nullptr;
  file.GetObject("histDiff", histogram);
  Require(histogram != nullptr, "missing histDiff");
  return histogram;
}
}  // namespace

int main() {
  try {
    const std::string directory =
        std::string(gSystem->TempDirectory()) + "/cshine_m11_spectrum_test_" +
        std::to_string(gSystem->GetPid());
    gSystem->mkdir(directory.c_str(), true);
    const std::string signal = directory + "/signal.root";
    const std::string background = directory + "/background.root";
    WriteInput(signal, 8.0, 10.0);
    WriteInput(background, 2.0, 20.0);
    const std::string slow = directory + "/nested/slow.root";
    const std::string slow_report = directory + "/nested/slow.tsv";
    const cshine_gamma::ObservedSpectrumSummary slow_summary =
        cshine_gamma::BuildObservedSpectrum(
            cshine_gamma::HistoricalObservedSpectrumDefinition(), signal,
            background, slow, slow_report,
            cshine_gamma::BackgroundSubtractionMode::kSlowBeamOff, false);
    Require(std::abs(slow_summary.background_scale - 0.5) < 1.0e-12,
            "slow-background normalization changed");
    TFile slow_file(slow.c_str(), "READ");
    TH1* slow_histogram = ReadDifference(slow_file);
    Require(slow_histogram->GetNbinsX() == 200 &&
                slow_histogram->GetXaxis()->GetXmin() == 0.0 &&
                slow_histogram->GetXaxis()->GetXmax() == 200.0,
            "observed spectrum rebinning changed");
    Require(std::abs(slow_histogram->GetBinContent(
                         slow_histogram->FindBin(40.1)) - 7.0) < 1.0e-12,
            "slow-background subtraction changed");
    slow_file.Close();
    const std::string fast = directory + "/nested/fast.root";
    const std::string fast_report = directory + "/nested/fast.tsv";
    const cshine_gamma::ObservedSpectrumSummary fast_summary =
        cshine_gamma::BuildObservedSpectrum(
            cshine_gamma::HistoricalObservedSpectrumDefinition(), signal,
            background, fast, fast_report,
            cshine_gamma::BackgroundSubtractionMode::kFastRandomWindow,
            false);
    Require(fast_summary.background_scale == 1.0,
            "equal-width fast-window scale changed");
    TFile fast_file(fast.c_str(), "READ");
    TH1* fast_histogram = ReadDifference(fast_file);
    Require(std::abs(fast_histogram->GetBinContent(
                         fast_histogram->FindBin(40.1)) - 6.0) < 1.0e-12,
            "fast-window direct subtraction changed");
    fast_file.Close();
    std::ifstream report_stream(slow_report.c_str());
    const std::string report_text((std::istreambuf_iterator<char>(report_stream)),
                                  std::istreambuf_iterator<char>());
    Require(report_text.find("slow-beam-off") != std::string::npos &&
                report_text.find("110:200") != std::string::npos,
            "observed-spectrum report is incomplete");
    bool overwrite_rejected = false;
    try {
      cshine_gamma::BuildObservedSpectrum(
          cshine_gamma::HistoricalObservedSpectrumDefinition(), signal,
          background, slow, slow_report,
          cshine_gamma::BackgroundSubtractionMode::kSlowBeamOff, false);
    } catch (const std::runtime_error&) {
      overwrite_rejected = true;
    }
    Require(overwrite_rejected, "existing observed spectrum was overwritten");
    std::cout << "Observed-spectrum synthetic test passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "test_observed_spectrum: " << error.what() << '\n';
    return 1;
  }
}
