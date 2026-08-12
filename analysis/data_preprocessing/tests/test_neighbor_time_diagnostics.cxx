#include "neighbor_time_diagnostics.h"

#include <TFile.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TSystem.h>
#include <TTree.h>

#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

namespace {

struct Event {
  std::array<Double_t, 15> energy;
  std::array<Double_t, 15> time;
};

bool Check(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAILED: " << message << '\n';
  }
  return condition;
}

bool NearlyEqual(double left, double right, double tolerance = 1.0e-12) {
  return std::abs(left - right) <= tolerance;
}

std::string TestDirectory() {
  return "/tmp/cshine_neighbor_time_diagnostics_test_" +
         std::to_string(static_cast<long long>(getpid()));
}

Event MakeEvent(double time5,
                double time6,
                double energy5,
                double energy6) {
  Event event = {};
  event.energy.fill(0.0);
  event.time.fill(0.0);
  event.energy[5] = energy5;
  event.energy[6] = energy6;
  event.time[5] = time5;
  event.time[6] = time6;
  return event;
}

void WriteEventTree(const std::string& path,
                    const std::vector<Event>& events,
                    bool omit_energy = false) {
  TFile output(path.c_str(), "RECREATE");
  TTree tree("GammaCaliData", "synthetic calibrated detector events");
  std::array<Double_t, 15> energy = {};
  std::array<Double_t, 15> time = {};
  tree.Branch("GammaTime", time.data(), "GammaTime[15]/D");
  if (!omit_energy) {
    tree.Branch("GammaEnergy", energy.data(), "GammaEnergy[15]/D");
  }
  for (std::vector<Event>::const_iterator event = events.begin();
       event != events.end(); ++event) {
    energy = event->energy;
    time = event->time;
    tree.Fill();
  }
  tree.Write();
  output.Close();
}

}  // namespace

int main() {
  bool ok = true;
  const std::string directory = TestDirectory();
  gSystem->mkdir(directory.c_str(), true);
  const std::string input_file = directory + "/events.root";
  const std::string missing_file = directory + "/missing_energy.root";
  const std::string output_file = directory + "/nested/diagnostics.root";
  const std::string report_file = directory + "/nested/diagnostics.tsv";
  const std::vector<Event> events = {
      MakeEvent(0.0, 0.0, 20.0, 15.0),
      MakeEvent(1.0, 1.0, 10.0, 10.0),
      MakeEvent(2.0, 2.0, 10.0, 10.0),
      MakeEvent(10.0, 0.0, 15.0, 15.0),
  };
  WriteEventTree(input_file, events);
  WriteEventTree(missing_file, events, true);

  const cshine_gamma::NeighborTimeDefinition definition =
      cshine_gamma::HistoricalCsI05CsI06NeighborTimeDefinition();
  ok &= Check(definition.input_tree_name == "GammaCaliData", "input tree");
  ok &= Check(definition.first_crystal == 5U &&
                  definition.second_crystal == 6U,
              "neighboring crystal indices");
  ok &= Check(NearlyEqual(definition.energy_threshold_mev, 30.0),
              "energy threshold");

  const cshine_gamma::NeighborTimeSummary summary =
      cshine_gamma::BuildNeighborTimeDiagnostics(
          definition, std::vector<std::string>{input_file}, output_file,
          report_file, false);
  ok &= Check(summary.resolved_input_files.size() == 1U, "resolved input");
  ok &= Check(summary.tree_entries == events.size(), "tree entries");
  ok &= Check(summary.time_correlation_all_rows == 4, "all 2D rows");
  ok &= Check(summary.time_correlation_cut_rows == 2, "selected 2D rows");
  ok &= Check(summary.time_difference_all_rows == 4, "all difference rows");
  ok &= Check(summary.time_difference_cut_rows == 2,
              "selected difference rows");
  ok &= Check(summary.first_time_rows == 4 && summary.second_time_rows == 4,
              "single-channel time rows");
  ok &= Check(NearlyEqual(summary.difference_all_peak, 3.0),
              "all difference peak");
  ok &= Check(NearlyEqual(summary.difference_cut_peak, 1.0),
              "selected difference peak");
  ok &= Check(NearlyEqual(summary.historical_peak_scale, 3.0),
              "historical display scale");

  TFile output(output_file.c_str(), "READ");
  TH2F* h2_all = nullptr;
  TH2F* h2_cut = nullptr;
  TH1F* h1 = nullptr;
  TH1F* hh_diff = nullptr;
  TH1F* h3 = nullptr;
  TH1F* h4 = nullptr;
  output.GetObject("h2_all", h2_all);
  output.GetObject("h2_cut", h2_cut);
  output.GetObject("h1", h1);
  output.GetObject("hh_diff", hh_diff);
  output.GetObject("h3", h3);
  output.GetObject("h4", h4);
  ok &= Check(h2_all != nullptr && h2_cut != nullptr, "2D object names");
  ok &= Check(h1 != nullptr && hh_diff != nullptr && h3 != nullptr &&
                  h4 != nullptr,
              "1D object names");
  if (h2_all != nullptr && h2_cut != nullptr) {
    ok &= Check(h2_all->GetNbinsX() == 100 && h2_all->GetNbinsY() == 100,
                "2D binning");
    ok &= Check(NearlyEqual(h2_all->GetXaxis()->GetXmin(), -500.0) &&
                    NearlyEqual(h2_all->GetXaxis()->GetXmax(), 500.0),
                "2D range");
    ok &= Check(NearlyEqual(h2_all->Integral(), 4.0), "all 2D integral");
    ok &= Check(NearlyEqual(h2_cut->Integral(), 2.0),
                "selected 2D integral");
  }
  if (h1 != nullptr && hh_diff != nullptr) {
    ok &= Check(h1->GetNbinsX() == 100 &&
                    NearlyEqual(h1->GetXaxis()->GetXmin(), -200.0) &&
                    NearlyEqual(h1->GetXaxis()->GetXmax(), 200.0),
                "difference binning");
    ok &= Check(NearlyEqual(h1->Integral(), 4.0), "all difference integral");
    ok &= Check(NearlyEqual(hh_diff->Integral(), 2.0),
                "selected difference integral");
    ok &= Check(NearlyEqual(hh_diff->GetMaximum(), 1.0),
                "stored selected histogram remains unscaled");
  }
  output.Close();

  std::ifstream report(report_file.c_str());
  const std::string report_text((std::istreambuf_iterator<char>(report)),
                                std::istreambuf_iterator<char>());
  ok &= Check(report_text.find("object\th2_all\tTH2F\t4\t4") !=
                  std::string::npos,
              "report all-correlation object");
  ok &= Check(report_text.find("summary\thistorical_peak_scale\t3") !=
                  std::string::npos,
              "report historical display scale");

  bool overwrite_rejected = false;
  try {
    cshine_gamma::BuildNeighborTimeDiagnostics(
        definition, std::vector<std::string>{input_file}, output_file,
        report_file, false);
  } catch (const std::runtime_error&) {
    overwrite_rejected = true;
  }
  ok &= Check(overwrite_rejected, "existing output rejection");

  bool missing_branch_rejected = false;
  try {
    cshine_gamma::BuildNeighborTimeDiagnostics(
        definition, std::vector<std::string>{missing_file},
        directory + "/missing.root", directory + "/missing.tsv", false);
  } catch (const std::runtime_error&) {
    missing_branch_rejected = true;
  }
  ok &= Check(missing_branch_rejected, "missing branch rejection");

  return ok ? 0 : 1;
}
