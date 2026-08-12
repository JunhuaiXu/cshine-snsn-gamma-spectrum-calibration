#include "observed_spectrum.h"

#include <TFile.h>
#include <TH1D.h>
#include <TH1F.h>
#include <TSystem.h>

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void Require(bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}

void WriteValidInput(const std::string& path) {
  TFile output(path.c_str(), "RECREATE");
  TH1D histogram("histDiff", "", 200, 0.0, 200.0);
  histogram.Sumw2();
  histogram.SetBinContent(1, 12.0);
  histogram.SetBinError(1, 3.0);
  histogram.SetBinContent(40, -2.0);
  histogram.SetBinError(40, 1.5);
  histogram.SetBinContent(0, 1.0);
  histogram.SetBinError(0, 1.0);
  histogram.SetBinContent(201, 4.0);
  histogram.SetBinError(201, 2.0);
  histogram.Write();
}

void WriteWrongClassInput(const std::string& path) {
  TFile output(path.c_str(), "RECREATE");
  TH1F histogram("histDiff", "", 200, 0.0, 200.0);
  histogram.Sumw2();
  histogram.Write();
}
}  // namespace

int main() {
  try {
    const std::string directory =
        std::string(gSystem->TempDirectory()) + "/cshine_m12_interface_test_" +
        std::to_string(gSystem->GetPid());
    gSystem->mkdir(directory.c_str(), true);
    const std::string input = directory + "/spectrum_110.root";
    const std::string report = directory + "/nested/interface.tsv";
    WriteValidInput(input);
    const cshine_gamma::ObservedSpectrumInterfaceSummary summary =
        cshine_gamma::InspectObservedSpectrumInterface(
            cshine_gamma::HistoricalObservedSpectrumInterfaceDefinition(),
            input, report, false);
    Require(summary.object_class == "TH1D", "interface class changed");
    Require(summary.bins == 200 && summary.bin_width_mev == 1.0,
            "interface binning changed");
    Require(summary.negative_regular_bins == 1,
            "negative bins must be reported without clipping");
    Require(summary.regular_bin_sum == 10.0 &&
                summary.regular_variance_sum == 11.25,
            "interface summary changed");
    Require(summary.underflow == 1.0 && summary.overflow == 4.0,
            "flow-bin reporting changed");
    std::ifstream report_stream(report.c_str());
    const std::string text((std::istreambuf_iterator<char>(report_stream)),
                           std::istreambuf_iterator<char>());
    Require(text.find("read_as_TH1D_or_TH1_base") != std::string::npos &&
                text.find("laboratory") != std::string::npos,
            "interface report is incomplete");

    bool overwrite_rejected = false;
    try {
      cshine_gamma::InspectObservedSpectrumInterface(
          cshine_gamma::HistoricalObservedSpectrumInterfaceDefinition(),
          input, report, false);
    } catch (const std::runtime_error&) {
      overwrite_rejected = true;
    }
    Require(overwrite_rejected, "existing interface report was overwritten");

    const std::string wrong_class = directory + "/wrong_class.root";
    WriteWrongClassInput(wrong_class);
    bool wrong_class_rejected = false;
    try {
      cshine_gamma::InspectObservedSpectrumInterface(
          cshine_gamma::HistoricalObservedSpectrumInterfaceDefinition(),
          wrong_class, directory + "/wrong_class.tsv", false);
    } catch (const std::runtime_error&) {
      wrong_class_rejected = true;
    }
    Require(wrong_class_rejected, "TH1F input was accepted as the TH1D interface");

    std::cout << "Observed-spectrum interface synthetic test passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "test_observed_spectrum_interface: " << error.what() << '\n';
    return 1;
  }
}
