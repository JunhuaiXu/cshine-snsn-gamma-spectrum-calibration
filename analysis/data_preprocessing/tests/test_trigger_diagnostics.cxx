#include "trigger_diagnostics.h"

#include "jiugong_recon.h"

#include <TFile.h>
#include <TH1.h>
#include <TH2.h>
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
  if (!condition) {
    throw std::runtime_error(message);
  }
}

jiugong_recon_result_t Candidate(unsigned short centre, double energy) {
  return jiugong_recon_result_t(
      centre, energy, std::map<unsigned short, double>{{centre, energy}});
}

void WriteInput(const std::string& path) {
  TFile output(path.c_str(), "RECREATE");
  TTree tree("GammaCaliData", "synthetic reconstructed events");
  std::array<UShort_t, 32> trigger_tdc = {};
  std::array<Double_t, 15> gamma_time = {};
  std::map<unsigned short, jiugong_recon_result_t> reconstructions;
  UShort_t count_veto = 0U;
  tree.Branch("TDC_Gamma_Trig_list", &trigger_tdc);
  tree.Branch("GammaTime", &gamma_time);
  tree.Branch("recon_result", &reconstructions);
  tree.Branch("count_veto", &count_veto, "count_veto/s");

  // Exact monitor boundaries are excluded from h1_TrigList but retained by
  // the five historical trigger-conditioned branches.
  trigger_tdc.fill(0U);
  trigger_tdc[16] = 300U;
  trigger_tdc[17] = 100U;
  trigger_tdc[18] = 4000U;
  trigger_tdc[22] = 200U;
  gamma_time.fill(std::numeric_limits<double>::quiet_NaN());
  gamma_time[5] = 10.0;
  reconstructions.clear();
  reconstructions[5] = Candidate(5U, 30.0);
  count_veto = 1U;
  tree.Fill();

  trigger_tdc.fill(0U);
  trigger_tdc[17] = 200U;
  trigger_tdc[19] = 300U;
  trigger_tdc[20] = 400U;
  trigger_tdc[22] = 500U;
  gamma_time.fill(std::numeric_limits<double>::quiet_NaN());
  gamma_time[4] = 20.0;
  reconstructions.clear();
  reconstructions[4] = Candidate(4U, 40.0);
  count_veto = 0U;
  tree.Fill();

  trigger_tdc.fill(0U);
  trigger_tdc[18] = 250U;
  gamma_time.fill(std::numeric_limits<double>::quiet_NaN());
  gamma_time[4] = 30.0;
  reconstructions.clear();
  reconstructions[4] = Candidate(4U, 50.0);
  count_veto = 1U;
  tree.Fill();

  tree.Write();
  output.Close();
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
        std::string(gSystem->TempDirectory()) + "/cshine_m10b_test_" +
        std::to_string(gSystem->GetPid());
    gSystem->mkdir(directory.c_str(), true);
    const std::string input = directory + "/m8.root";
    const std::string output = directory + "/nested/trigger.root";
    const std::string report = directory + "/nested/trigger.run.tsv";
    WriteInput(input);

    const cshine_gamma::TriggerDiagnosticsSummary summary =
        cshine_gamma::BuildTriggerDiagnostics(
            cshine_gamma::HistoricalTriggerDiagnosticsDefinition(),
            std::vector<std::string>(1, input), output, report, false);
    Require(summary.input_entries == 3U, "input-entry count changed");
    Require(summary.monitor_entries[0] == 1U &&
                summary.monitor_entries[1] == 1U &&
                summary.monitor_entries[2] == 1U &&
                summary.monitor_entries[3] == 1U &&
                summary.monitor_entries[4] == 1U &&
                summary.monitor_entries[6] == 2U,
            "strict monitor-window mapping changed");
    Require(summary.conditioned_event_entries ==
                std::array<unsigned long long, 5>{{2U, 2U, 1U, 1U, 2U}},
            "inclusive trigger-condition mapping changed");
    Require(summary.historical_candidate_entries ==
                std::array<unsigned long long, 5>{{1U, 1U, 1U, 1U, 1U}},
            "frozen Figure 15 selection changed");
    Require(summary.reviewed_candidate_entries ==
                std::array<unsigned long long, 5>{{2U, 1U, 1U, 1U, 2U}},
            "reviewed main selection changed");

    TFile result(output.c_str(), "READ");
    for (unsigned int monitor = 0; monitor < 15U; ++monitor) {
      TH1* histogram = Histogram(
          result, ("h1_TrigList" + std::to_string(monitor)).c_str());
      Require(histogram->GetNbinsX() == 4096 &&
                  histogram->GetXaxis()->GetXmin() == 0.0 &&
                  histogram->GetXaxis()->GetXmax() == 4096.0,
              "monitor histogram schema changed");
    }
    TH2* historical_17 = dynamic_cast<TH2*>(
        Histogram(result, "historical_h2_TOF_TotalE_Trig17"));
    TH2* reviewed_17 = dynamic_cast<TH2*>(
        Histogram(result, "reviewed_h2_TOF_TotalE_Trig17"));
    Require(historical_17 != nullptr && reviewed_17 != nullptr &&
                historical_17->GetEntries() == 1.0 &&
                reviewed_17->GetEntries() == 2.0,
            "dual selection outputs changed");
    Require(historical_17->GetNbinsX() == 100 &&
                historical_17->GetNbinsY() == 200,
            "conditioned histogram schema changed");
    result.Close();

    std::ifstream report_stream(report.c_str());
    const std::string report_text((std::istreambuf_iterator<char>(report_stream)),
                                  std::istreambuf_iterator<char>());
    Require(report_text.find("historical_figure15_selection") !=
                std::string::npos &&
                report_text.find("reviewed_main_selection") !=
                    std::string::npos,
            "run report does not expose both policies");
    std::cout << "Trigger diagnostic synthetic test passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "test_trigger_diagnostics: " << error.what() << '\n';
    return 1;
  }
}
