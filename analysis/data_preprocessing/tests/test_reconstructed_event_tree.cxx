#include "reconstructed_event_tree.h"

#include "jiugong_recon.h"

#include <TFile.h>
#include <TH1.h>
#include <TSystem.h>
#include <TTree.h>
#include <TTreeReader.h>
#include <TTreeReaderArray.h>
#include <TTreeReaderValue.h>

#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

namespace {

struct Event {
  std::array<Double_t, 15> energy;
  std::array<Double_t, 15> time;
  std::array<UShort_t, 32> adc;
  std::array<UShort_t, 32> tdc;
  std::array<UShort_t, 32> trigger_tdc;
  std::array<Int_t, 3> veto_adc;
  std::array<Int_t, 3> veto_tdc;
  std::array<UShort_t, 4> t0;
};

bool Check(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAILED: " << message << '\n';
  }
  return condition;
}

std::string TestDirectory() {
  return "/tmp/cshine_reconstructed_event_tree_test_" +
         std::to_string(static_cast<long long>(getpid()));
}

Event EmptyEvent() {
  Event event = {};
  event.energy.fill(0.0);
  event.time.fill(std::numeric_limits<double>::quiet_NaN());
  return event;
}

void WriteCalibratedTree(const std::string& path,
                         const std::vector<Event>& events,
                         bool omit_veto_time = false) {
  TFile output(path.c_str(), "RECREATE");
  TTree tree("GammaCaliData", "synthetic calibrated detector events");
  Event event = {};
  tree.Branch("GammaEnergy", &event.energy);
  tree.Branch("GammaTime", &event.time);
  tree.Branch("ADC_Gamma", &event.adc);
  tree.Branch("TDC_Gamma", &event.tdc);
  tree.Branch("TDC_Gamma_Trig_list", &event.trigger_tdc);
  tree.Branch("ADC_Veto", &event.veto_adc);
  if (!omit_veto_time) {
    tree.Branch("TDC_Veto", &event.veto_tdc);
  }
  tree.Branch("TDC_T0", &event.t0);
  for (std::vector<Event>::const_iterator input = events.begin();
       input != events.end(); ++input) {
    event = *input;
    tree.Fill();
  }
  tree.Write();
}

}  // namespace

int main() {
  bool ok = true;
  const std::string directory = TestDirectory();
  gSystem->mkdir(directory.c_str(), true);
  const std::string input_path = directory + "/calibrated.root";
  const std::string output_path = directory + "/nested/reconstructed.root";
  const std::string report_path = directory + "/nested/reconstructed.tsv";
  const std::string missing_path = directory + "/missing.root";

  Event central = EmptyEvent();
  central.energy[5] = 10.0;
  central.energy[6] = 2.0;
  central.time[5] = 0.0;
  central.time[6] = 50.0;
  central.veto_tdc = {{101, 200, 3999}};
  central.adc[5] = 105U;
  central.trigger_tdc[17] = 1234U;

  Event side_silent = EmptyEvent();
  side_silent.energy[7] = 8.0;
  side_silent.time[7] = 0.0;
  side_silent.veto_tdc = {{100, 4000, 0}};

  Event side_vetoed = EmptyEvent();
  side_vetoed.energy[7] = 9.0;
  side_vetoed.time[7] = 0.0;
  side_vetoed.veto_tdc = {{100, 101, 4000}};

  Event placeholder = EmptyEvent();
  placeholder.energy[5] = 10.0;
  placeholder.time[5] = 0.0;
  placeholder.energy[6] = 2.0;
  placeholder.time[6] = 75.0;

  Event empty = EmptyEvent();
  WriteCalibratedTree(input_path,
                      {central, side_silent, side_vetoed, placeholder, empty});
  WriteCalibratedTree(missing_path, {central}, true);

  const cshine_gamma::ReconstructedEventSummary summary =
      cshine_gamma::BuildReconstructedEventTree(
          cshine_gamma::HistoricalReconstructedEventDefinition(),
          {input_path}, output_path, report_path, false);
  ok &= Check(summary.input_entries == 5U && summary.output_entries == 5U,
              "input and output entries");
  ok &= Check(summary.entries_without_reconstruction == 1U,
              "empty event count");
  ok &= Check(summary.valid_reconstruction_count == 4U,
              "valid reconstruction count");
  ok &= Check(summary.placeholder_reconstruction_count == 1U,
              "placeholder reconstruction count");
  ok &= Check(summary.main_spectrum_candidate_count == 3U,
              "central plus silent-side main candidates");
  ok &= Check(summary.side_candidate_rejected_by_veto_count == 1U,
              "vetoed side candidate count");
  ok &= Check(summary.events_by_veto_signal_count[0] == 3U &&
                  summary.events_by_veto_signal_count[1] == 1U &&
                  summary.events_by_veto_signal_count[3] == 1U,
              "veto multiplicity accounting");

  TFile output(output_path.c_str(), "READ");
  TTree* tree = nullptr;
  output.GetObject("GammaCaliData", tree);
  ok &= Check(tree != nullptr, "output GammaCaliData tree");
  if (tree != nullptr) {
    for (const char* branch : {"GammaEnergy", "GammaTime", "ADC_Gamma",
                               "TDC_Gamma", "ADC_Veto", "TDC_Veto",
                               "TDC_Gamma_Trig_list", "TDC_T0",
                               "recon_result", "count_veto"}) {
      ok &= Check(tree->GetBranch(branch) != nullptr,
                  std::string("output branch ") + branch);
    }
    TTreeReader reader(tree);
    TTreeReaderValue<std::map<unsigned short, jiugong_recon_result_t> > result(
        reader, "recon_result");
    TTreeReaderValue<UShort_t> count_veto(reader, "count_veto");
    TTreeReaderArray<UShort_t> trigger_tdc(reader,
                                          "TDC_Gamma_Trig_list");
    unsigned int entry = 0U;
    while (reader.Next()) {
      if (entry == 0U) {
        ok &= Check(result->at(5U).GetMultiplicity() == 2U &&
                        std::abs(result->at(5U).GetEnergy() - 12.0) < 1.0e-12,
                    "central shower contents");
        ok &= Check(*count_veto == 3U, "three-face veto count");
        ok &= Check(trigger_tdc[17] == 1234U,
                    "hardware trigger-monitor TDC propagated");
      }
      if (entry == 3U) {
        ok &= Check(result->count(6U) == 1U &&
                        result->at(6U).GetCenter() == -1,
                    "placeholder streamed in output tree");
      }
      ++entry;
    }
    ok &= Check(entry == 5U, "read all reconstructed entries");
  }
  TH1* central_all = nullptr;
  TH1* detector_energy = nullptr;
  TH1* central_silent = nullptr;
  TH1* central_vetoed = nullptr;
  TH1* side_all = nullptr;
  TH1* side_silent_histogram = nullptr;
  TH1* side_vetoed_histogram = nullptr;
  output.GetObject("h_recon_5", central_all);
  output.GetObject("h_eDep_5", detector_energy);
  output.GetObject("h_recon_veto_5", central_silent);
  output.GetObject("h_recon_vetoed_5", central_vetoed);
  output.GetObject("h_recon_7", side_all);
  output.GetObject("h_recon_veto_7", side_silent_histogram);
  output.GetObject("h_recon_vetoed_7", side_vetoed_histogram);
  ok &= Check(detector_energy != nullptr && central_all != nullptr &&
                  central_silent != nullptr &&
                  central_vetoed != nullptr && side_all != nullptr &&
                  side_silent_histogram != nullptr &&
                  side_vetoed_histogram != nullptr,
              "per-crystal reconstructed histogram families");
  ok &= Check(detector_energy != nullptr && detector_energy->GetEntries() == 2.0 &&
                  detector_energy->GetNbinsX() == 1000 &&
                  std::abs(detector_energy->GetXaxis()->GetXmax() - 100.0) <
                      1.0e-12,
              "historical detector-energy histogram");
  if (central_all != nullptr && central_silent != nullptr &&
      central_vetoed != nullptr) {
    ok &= Check(central_all->GetEntries() == 2.0 &&
                    central_silent->GetEntries() == 1.0 &&
                    central_vetoed->GetEntries() == 1.0,
                "central reconstructed spectra retain veto split");
  }
  if (side_all != nullptr && side_silent_histogram != nullptr &&
      side_vetoed_histogram != nullptr) {
    ok &= Check(side_all->GetEntries() == 2.0 &&
                    side_silent_histogram->GetEntries() == 1.0 &&
                    side_vetoed_histogram->GetEntries() == 1.0,
                "side reconstructed spectra retain veto split");
  }
  TH1* placeholder_histogram = nullptr;
  output.GetObject("h_recon_6", placeholder_histogram);
  ok &= Check(placeholder_histogram != nullptr &&
                  placeholder_histogram->GetEntries() == 1.0 &&
                  placeholder_histogram->GetBinContent(1) == 1.0,
              "historical placeholder contributes at zero energy");

  std::ifstream report(report_path.c_str());
  const std::string report_text((std::istreambuf_iterator<char>(report)),
                                std::istreambuf_iterator<char>());
  ok &= Check(report_text.find("config\tneighbor_time_ns\t50") !=
                  std::string::npos,
              "report records neighbor time");
  ok &= Check(report_text.find("summary\tmain_spectrum_candidates\t3") !=
                  std::string::npos,
              "report records main candidates");

  bool overwrite_rejected = false;
  try {
    cshine_gamma::BuildReconstructedEventTree(
        cshine_gamma::HistoricalReconstructedEventDefinition(), {input_path},
        output_path, report_path, false);
  } catch (const std::runtime_error&) {
    overwrite_rejected = true;
  }
  ok &= Check(overwrite_rejected, "existing output rejection");

  bool missing_branch_rejected = false;
  try {
    cshine_gamma::BuildReconstructedEventTree(
        cshine_gamma::HistoricalReconstructedEventDefinition(), {missing_path},
        directory + "/missing-output.root", directory + "/missing-output.tsv",
        false);
  } catch (const std::runtime_error&) {
    missing_branch_rejected = true;
  }
  ok &= Check(missing_branch_rejected, "missing veto branch rejection");
  return ok ? 0 : 1;
}
