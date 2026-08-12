#include "fast_coincidence_spectra.h"
#include "jiugong_recon.h"

#include <TFile.h>
#include <TH1.h>
#include <TSystem.h>
#include <TTree.h>

#include <array>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void Require(bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}
jiugong_recon_result_t Candidate(unsigned short center, double energy) {
  return jiugong_recon_result_t(
      center, energy, std::map<unsigned short, double>{{center, energy}});
}
void WriteInput(const std::string& path) {
  TFile output(path.c_str(), "RECREATE");
  TTree tree("GammaCaliData", "synthetic M8 events");
  std::array<UShort_t, 32> trigger = {};
  std::array<Double_t, 15> time = {};
  std::map<unsigned short, jiugong_recon_result_t> reconstructions;
  UShort_t veto_count = 0U;
  tree.Branch("TDC_Gamma_Trig_list", &trigger);
  tree.Branch("GammaTime", &time);
  tree.Branch("recon_result", &reconstructions);
  tree.Branch("count_veto", &veto_count, "count_veto/s");
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const auto fill = [&](unsigned short trigger18, unsigned short core,
                        double core_time, double energy,
                        unsigned short veto) {
    trigger.fill(0U);
    trigger[18] = trigger18;
    time.fill(nan);
    time[core] = core_time;
    reconstructions.clear();
    reconstructions[core] = Candidate(core, energy);
    veto_count = veto;
    tree.Fill();
  };
  fill(840U, 5U, -100.0, 10.0, 0U);
  fill(835U, 5U, -350.0, 20.0, 2U);
  fill(850U, 6U, -50.0, 30.0, 1U);
  fill(0U, 4U, 50.0, 40.0, 0U);
  fill(0U, 7U, 350.0, 50.0, 0U);
  fill(0U, 8U, -100.0, 60.0, 1U);
  fill(0U, 0U, -100.0, 70.0, 0U);
  fill(0U, 5U, 0.0, 80.0, 0U);
  tree.Write();
}
TH1* Histogram(TFile& file, const char* name) {
  TH1* histogram = nullptr;
  file.GetObject(name, histogram);
  Require(histogram != nullptr, std::string("missing histogram: ") + name);
  return histogram;
}
}  // namespace

int main() {
  try {
    const std::string directory =
        std::string(gSystem->TempDirectory()) + "/cshine_m11_fast_test_" +
        std::to_string(gSystem->GetPid());
    gSystem->mkdir(directory.c_str(), true);
    const std::string input = directory + "/m8.root";
    const std::string signal = directory + "/nested/fast-window.root";
    const std::string random = directory + "/nested/random-window.root";
    const std::string report = directory + "/nested/fast.run.tsv";
    WriteInput(input);
    const cshine_gamma::FastCoincidenceSummary summary =
        cshine_gamma::BuildFastCoincidenceSpectra(
            cshine_gamma::HistoricalFastCoincidenceDefinition(),
            std::vector<std::string>(1, input), signal, random, report, false);
    Require(summary.input_entries == 8U, "input entry count changed");
    Require(summary.excluded_ssd_m2_entries == 1U,
            "strict SSD M2 exclusion changed");
    Require(summary.signal_central_candidates == 2U &&
                summary.signal_side_candidates == 0U,
            "signal-window candidate policy changed");
    Require(summary.random_central_candidates == 0U &&
                summary.random_side_candidates == 2U,
            "random-window candidate policy changed");
    TFile signal_file(signal.c_str(), "READ");
    TFile random_file(random.c_str(), "READ");
    for (TFile* file : {&signal_file, &random_file}) {
      for (const char* name : {"h_central_E_M1", "h_side_E_M1",
                               "h_total_E_M1"}) {
        TH1* histogram = Histogram(*file, name);
        Require(histogram->GetNbinsX() == 1000 &&
                    histogram->GetXaxis()->GetXmin() == 0.0 &&
                    histogram->GetXaxis()->GetXmax() == 200.0,
                "fast-window histogram schema changed");
      }
    }
    Require(Histogram(signal_file, "h_total_E_M1")->Integral() == 2.0,
            "signal total changed");
    Require(Histogram(random_file, "h_total_E_M1")->Integral() == 2.0,
            "random total changed");
    std::ifstream report_stream(report.c_str());
    const std::string report_text((std::istreambuf_iterator<char>(report_stream)),
                                  std::istreambuf_iterator<char>());
    Require(report_text.find("strict_835:850") != std::string::npos &&
                report_text.find("inclusive\t-350:-50") != std::string::npos &&
                report_text.find("inclusive\t50:350") != std::string::npos,
            "report does not expose the fast-window boundaries");
    bool overwrite_rejected = false;
    try {
      cshine_gamma::BuildFastCoincidenceSpectra(
          cshine_gamma::HistoricalFastCoincidenceDefinition(),
          std::vector<std::string>(1, input), signal, random, report, false);
    } catch (const std::runtime_error&) {
      overwrite_rejected = true;
    }
    Require(overwrite_rejected, "existing fast outputs were overwritten");
    std::cout << "Fast-coincidence spectrum synthetic test passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "test_fast_coincidence_spectra: " << error.what() << '\n';
    return 1;
  }
}
