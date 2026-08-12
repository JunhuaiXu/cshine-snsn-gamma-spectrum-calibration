#include "chapter3_diagnostics.h"

#include "jiugong_recon.h"

#include <TFile.h>
#include <TH1.h>
#include <TH2.h>
#include <TSystem.h>
#include <TTree.h>

#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

jiugong_recon_result_t Candidate(
    unsigned short centre,
    const std::map<unsigned short, double>& crystals) {
  double total = 0.0;
  for (std::map<unsigned short, double>::const_iterator item = crystals.begin();
       item != crystals.end(); ++item) {
    total += item->second;
  }
  return jiugong_recon_result_t(centre, total, crystals);
}

void WriteInput(const std::string& path) {
  TFile output(path.c_str(), "RECREATE");
  TTree tree("GammaCaliData", "synthetic reconstructed events");
  std::array<Double_t, 15> energy = {};
  std::array<Double_t, 15> time = {};
  std::map<unsigned short, jiugong_recon_result_t> reconstructions;
  UShort_t count_veto = 0U;
  tree.Branch("GammaEnergy", &energy);
  tree.Branch("GammaTime", &time);
  tree.Branch("recon_result", &reconstructions);
  tree.Branch("count_veto", &count_veto, "count_veto/s");

  energy.fill(0.0);
  time.fill(std::numeric_limits<double>::quiet_NaN());
  energy[5] = 20.0;
  energy[6] = 10.0;
  time[5] = 10.0;
  reconstructions.clear();
  reconstructions[5] = Candidate(5, {{5, 20.0}, {6, 10.0}});
  count_veto = 1U;
  tree.Fill();

  energy.fill(0.0);
  time.fill(std::numeric_limits<double>::quiet_NaN());
  energy[4] = 30.0;
  energy[5] = 20.0;
  time[4] = 20.0;
  reconstructions.clear();
  reconstructions[4] = Candidate(4, {{4, 30.0}, {5, 20.0}});
  count_veto = 1U;
  tree.Fill();

  energy.fill(0.0);
  time.fill(std::numeric_limits<double>::quiet_NaN());
  energy[4] = 30.0;
  energy[5] = 20.0;
  energy[10] = 40.0;
  time[4] = 30.0;
  time[10] = 40.0;
  reconstructions.clear();
  reconstructions[4] = Candidate(4, {{4, 30.0}, {5, 20.0}});
  reconstructions[10] = Candidate(10, {{10, 40.0}});
  count_veto = 0U;
  tree.Fill();

  tree.Write();
  output.Close();
}

TH1* RequireHistogram(TFile& file, const char* name) {
  TH1* histogram = nullptr;
  file.GetObject(name, histogram);
  Require(histogram != nullptr, std::string("missing histogram: ") + name);
  return histogram;
}

}  // namespace

int main() {
  try {
    const std::string directory =
        std::string(gSystem->TempDirectory()) + "/cshine_m10_test_" +
        std::to_string(gSystem->GetPid());
    gSystem->mkdir(directory.c_str(), true);
    const std::string input = directory + "/m8.root";
    const std::string output = directory + "/diagnostics.root";
    const std::string report = directory + "/diagnostics.run.tsv";
    const std::string background_output = directory + "/diagnostics_bkg.root";
    const std::string background_report = directory + "/diagnostics_bkg.run.tsv";
    WriteInput(input);

    const cshine_gamma::Chapter3DiagnosticsSummary summary =
        cshine_gamma::BuildChapter3Diagnostics(
            cshine_gamma::HistoricalChapter3DiagnosticsDefinition(),
            std::vector<std::string>(1, input), output, report, "beam-on", false);
    Require(summary.input_entries == 3U, "wrong input-entry count");
    Require(summary.valid_reconstruction_count == 4U,
            "wrong valid-reconstruction count");
    Require(summary.main_candidate_count == 3U, "wrong main-candidate count");
    Require(summary.central_candidate_count == 2U,
            "wrong central-candidate count");
    Require(summary.side_candidate_count == 1U, "wrong side-candidate count");
    Require(summary.side_candidate_rejected_by_veto_count == 1U,
            "side veto selection changed");
    Require(summary.all15_candidate_count == 4U,
            "all-15 multiplicity selection changed");

    TFile result(output.c_str(), "READ");
    TH1* delta_y = RequireHistogram(result, "ALL_h2_TotalE_DeltaY");
    TH1* low = RequireHistogram(result, "ALL_h2_ax_ay_10_100");
    TH1* high = RequireHistogram(result, "ALL_h2_ax_ay_100_inf");
    TH1* core_time = RequireHistogram(result, "ALL_h2_TOF_TotalE");
    TH1* all_count = RequireHistogram(result, "all15_core_multiplicity");
    TH1* high_count = RequireHistogram(result, "all15_high_core_multiplicity");
    Require(delta_y->GetEntries() == 3.0, "main selection histogram changed");
    Require(low->GetEntries() == 3.0 && high->GetEntries() == 0.0,
            "energy-interval boundary changed");
    Require(core_time->GetNbinsX() == 100 &&
                dynamic_cast<TH2*>(core_time)->GetNbinsY() == 200,
            "published time-energy object schema changed");
    Require(all_count->GetBinContent(1) == 2.0 &&
                all_count->GetBinContent(2) == 1.0,
            "all-15 trigger multiplicity changed");
    Require(high_count->GetBinContent(1) == 1.0 &&
                high_count->GetBinContent(2) == 1.0,
            "all-15 high-energy multiplicity changed");
    result.Close();

    const cshine_gamma::Chapter3DiagnosticsSummary background_summary =
        cshine_gamma::BuildChapter3Diagnostics(
            cshine_gamma::HistoricalChapter3DiagnosticsDefinition(),
            std::vector<std::string>(1, input), background_output,
            background_report, "beam-off", false);
    Require(background_summary.main_candidate_count == 3U,
            "beam-off main selection changed");
    TFile background_result(background_output.c_str(), "READ");
    TH2* background_time = dynamic_cast<TH2*>(
        RequireHistogram(background_result, "ALL_h2_TOF_TotalE"));
    Require(background_time != nullptr && background_time->GetMean(1) < 0.0,
            "historical beam-off time-sign convention changed");
    background_result.Close();

    std::ifstream report_stream(report.c_str());
    Require(static_cast<bool>(report_stream), "missing run report");
    gSystem->Unlink(input.c_str());
    gSystem->Unlink(output.c_str());
    gSystem->Unlink(report.c_str());
    gSystem->Unlink(background_output.c_str());
    gSystem->Unlink(background_report.c_str());
    std::cout << "Chapter 3 diagnostic synthetic test passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "test_chapter3_diagnostics: " << error.what() << '\n';
    return 1;
  }
}
