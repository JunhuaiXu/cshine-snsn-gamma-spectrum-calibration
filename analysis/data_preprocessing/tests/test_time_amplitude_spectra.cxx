#include "time_amplitude_spectra.h"

#include <TFile.h>
#include <TH2I.h>
#include <TKey.h>
#include <TSystem.h>
#include <TTree.h>

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Event {
  Int_t low_energy_0;
  Int_t gamma_time_0;
  Int_t low_energy_1;
  Int_t gamma_time_1;
  Int_t t0_time_0;
  Int_t t0_time_1;
};

bool NearlyEqual(double left, double right, double tolerance = 1.0e-12) {
  return std::abs(left - right) <= tolerance;
}

bool Check(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAILED: " << message << '\n';
  }
  return condition;
}

void WriteSyntheticTree(const std::string& path,
                        const std::vector<Event>& events,
                        bool include_second_t0 = true) {
  TFile output(path.c_str(), "RECREATE");
  if (output.IsZombie()) {
    throw std::runtime_error("cannot create synthetic ROOT file");
  }
  TTree tree("tree", "");
  Event event = {};
  tree.Branch("GAMMA1_LOW_E", &event.low_energy_0, "GAMMA1_LOW_E/I");
  tree.Branch("GAMMA1_T", &event.gamma_time_0, "GAMMA1_T/I");
  tree.Branch("GAMMA2_LOW_E", &event.low_energy_1, "GAMMA2_LOW_E/I");
  tree.Branch("GAMMA2_T", &event.gamma_time_1, "GAMMA2_T/I");
  tree.Branch("T01_T", &event.t0_time_0, "T01_T/I");
  if (include_second_t0) {
    tree.Branch("T02_T", &event.t0_time_1, "T02_T/I");
  }
  for (std::vector<Event>::const_iterator item = events.begin();
       item != events.end(); ++item) {
    event = *item;
    tree.Fill();
  }
  tree.Write();
  output.Close();
}

std::string ReadTextFile(const std::string& path) {
  std::ifstream input(path.c_str());
  std::ostringstream content;
  content << input.rdbuf();
  return content.str();
}

cshine_gamma::TimeAmplitudeDefinition SyntheticDefinition(
    cshine_gamma::TimeAmplitudeMode mode) {
  cshine_gamma::TimeAmplitudeDefinition definition =
      mode == cshine_gamma::TimeAmplitudeMode::kOriginal
          ? cshine_gamma::CentralOriginalTimeAmplitudeDefinition()
          : cshine_gamma::CentralHistoricalCorrectedTimeAmplitudeDefinition();
  definition.name = "synthetic";
  definition.input_patterns.clear();
  definition.input_patterns.push_back("synthetic.root");
  definition.channel_count = 2;
  definition.t0_count = 2;
  definition.thread_count = 1;
  definition.time_bin_count = 20;
  definition.time_min_ns = 0.0;
  definition.time_max_ns = 2000.0;
  definition.adc_bin_count = 16;
  return definition;
}

}  // namespace

int main() {
  bool ok = true;

  const cshine_gamma::TimeAmplitudeDefinition central =
      cshine_gamma::CentralOriginalTimeAmplitudeDefinition();
  ok &= Check(central.input_patterns.size() == 9 &&
                  central.input_patterns.front() ==
                      "a20240303_SnSn_ALLCOIN.*.root" &&
                  central.input_patterns.back() ==
                      "a20240310_SnSn_GOAL_ALLCOIN.*.root",
              "historical 0303--0310 input patterns");
  ok &= Check(central.channel_count == 15 && central.t0_count == 4 &&
                  central.thread_count == 12,
              "historical channel and thread counts");
  ok &= Check(central.tdc_min_inclusive == 100 &&
                  central.tdc_max_inclusive == 4000,
              "inclusive histogram-production TDC bounds");
  ok &= Check(central.time_bin_count == 1000 &&
                  central.adc_bin_count == 2048 &&
                  NearlyEqual(central.time_max_ns,
                              409.6 + (8.9 / 30.0) * 4096.0),
              "historical original histogram schema");
  const cshine_gamma::TimeAmplitudeDefinition corrected =
      cshine_gamma::CentralHistoricalCorrectedTimeAmplitudeDefinition();
  ok &= Check(corrected.time_bin_count == 500 &&
                  corrected.adc_bin_count == 512 &&
                  corrected.mode ==
                      cshine_gamma::TimeAmplitudeMode::
                          kHistoricalDiagnosticCorrected,
              "historical corrected diagnostic schema");
  ok &= Check(cshine_gamma::LowGainEnergyBranchName(0) == "GAMMA1_LOW_E" &&
                  cshine_gamma::GammaTimeBranchName(14) == "GAMMA15_T" &&
                  cshine_gamma::T0TimeBranchName(3) == "T04_T",
              "historical input branch names");
  ok &= Check(cshine_gamma::IndividualT0HistogramName(5, 3) ==
                      "h_TOF_one_5_c3" &&
                  cshine_gamma::AverageT0HistogramName(
                      cshine_gamma::TimeAmplitudeMode::kOriginal, 5) ==
                      "h_TOF_move_5" &&
                  cshine_gamma::AverageT0HistogramName(
                      cshine_gamma::TimeAmplitudeMode::
                          kHistoricalDiagnosticCorrected,
                      5) == "h_TOF_move_cali_5",
              "historical output object names");

  std::ostringstream directory_name;
  directory_name << gSystem->TempDirectory()
                 << "/cshine_gamma_time_amplitude_" << gSystem->GetPid();
  const std::string directory = directory_name.str();
  gSystem->mkdir(directory.c_str(), true);
  const std::string input_path = directory + "/synthetic.root";
  const std::vector<Event> events = {
      {100, 100, 200, 4000, 100, 4000},
      {300, 200, 400, 300, 99, 4001},
      {500, 99, 600, 200, 300, 99},
  };
  WriteSyntheticTree(input_path, events);

  const std::string output_directory = directory + "/nested/m5";
  const std::string original_output = output_directory + "/time_orig.root";
  const std::string original_report = output_directory + "/time_orig.run.tsv";
  cshine_gamma::TimeAmplitudeDefinition synthetic = SyntheticDefinition(
      cshine_gamma::TimeAmplitudeMode::kOriginal);
  const cshine_gamma::TimeAmplitudeSummary original_summary =
      cshine_gamma::BuildTimeAmplitudeSpectra(
          synthetic, directory, original_output, original_report, false);
  ok &= Check(original_summary.input_file_count == 1 &&
                  original_summary.tree_entries == 3,
              "synthetic input summary");
  ok &= Check(original_summary.average_t0_histogram_entries.size() == 2 &&
                  original_summary.average_t0_histogram_entries[0] == 1.0 &&
                  original_summary.average_t0_histogram_entries[1] == 2.0,
              "inclusive gamma TDC selection and T0-available selection");
  ok &= Check(original_summary.individual_t0_histogram_entries[0][0] == 1.0 &&
                  original_summary.individual_t0_histogram_entries[0][1] ==
                      1.0 &&
                  original_summary.individual_t0_histogram_entries[1][0] ==
                      2.0 &&
                  original_summary.individual_t0_histogram_entries[1][1] ==
                      1.0,
              "per-T0 and average-T0 fill contracts");

  TFile original_file(original_output.c_str(), "READ");
  TH2I* original_move_0 = nullptr;
  TH2I* original_one_1_0 = nullptr;
  original_file.GetObject("h_TOF_move_0", original_move_0);
  original_file.GetObject("h_TOF_one_1_c0", original_one_1_0);
  ok &= Check(original_move_0 != nullptr && original_one_1_0 != nullptr,
              "original output objects are readable TH2I");
  if (original_move_0 != nullptr) {
    ok &= Check(original_move_0->GetNbinsX() == 20 &&
                    original_move_0->GetNbinsY() == 16 &&
                    original_move_0->GetEntries() == 1.0,
                "synthetic original object schema and entries");
  }
  ok &= Check(original_file.GetListOfKeys()->GetEntries() == 6,
              "two channels produce four per-T0 and two average objects");
  original_file.Close();

  const std::string corrected_output = output_directory + "/time_cali.root";
  const std::string corrected_report = output_directory + "/time_cali.run.tsv";
  synthetic = SyntheticDefinition(
      cshine_gamma::TimeAmplitudeMode::kHistoricalDiagnosticCorrected);
  const cshine_gamma::TimeAmplitudeSummary corrected_summary =
      cshine_gamma::BuildTimeAmplitudeSpectra(
          synthetic, directory, corrected_output, corrected_report, false);
  ok &= Check(corrected_summary.average_t0_histogram_entries[0] == 1.0 &&
                  corrected_summary.average_t0_histogram_entries[1] == 2.0,
              "historical corrected mode preserves event selection");
  TFile corrected_file(corrected_output.c_str(), "READ");
  TH2I* corrected_move_0 = nullptr;
  corrected_file.GetObject("h_TOF_move_cali_0", corrected_move_0);
  ok &= Check(corrected_move_0 != nullptr,
              "historical corrected object name is preserved");
  corrected_file.Close();

  const std::string original_report_text = ReadTextFile(original_report);
  ok &= Check(original_report_text.find(
                  "config\ttdc_inclusive\t100\t4000") != std::string::npos &&
                  original_report_text.find(
                      "input_pattern\tsynthetic.root\t1") !=
                      std::string::npos,
              "run report records boundaries and relative input pattern");

  bool overwrite_rejected = false;
  try {
    cshine_gamma::BuildTimeAmplitudeSpectra(
        synthetic, directory, corrected_output, corrected_report, false);
  } catch (const std::runtime_error&) {
    overwrite_rejected = true;
  }
  ok &= Check(overwrite_rejected, "existing outputs are protected");

  const std::string missing_input = directory + "/missing_t0.root";
  WriteSyntheticTree(missing_input, events, false);
  synthetic.input_patterns.clear();
  synthetic.input_patterns.push_back("missing_t0.root");
  bool missing_branch_rejected = false;
  try {
    cshine_gamma::BuildTimeAmplitudeSpectra(
        synthetic, directory, output_directory + "/missing.root",
        output_directory + "/missing.run.tsv", false);
  } catch (const std::runtime_error&) {
    missing_branch_rejected = true;
  }
  ok &= Check(missing_branch_rejected, "missing T0 branch is rejected");

  gSystem->Unlink(input_path.c_str());
  gSystem->Unlink(missing_input.c_str());
  gSystem->Unlink(original_output.c_str());
  gSystem->Unlink(original_report.c_str());
  gSystem->Unlink(corrected_output.c_str());
  gSystem->Unlink(corrected_report.c_str());
  std::remove(output_directory.c_str());
  std::remove((directory + "/nested").c_str());
  std::remove(directory.c_str());

  return ok ? 0 : 1;
}
