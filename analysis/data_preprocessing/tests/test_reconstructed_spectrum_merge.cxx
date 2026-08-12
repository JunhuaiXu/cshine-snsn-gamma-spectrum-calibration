#include "reconstructed_spectrum_merge.h"

#include <TCanvas.h>
#include <TFile.h>
#include <TH1.h>
#include <TH1I.h>
#include <TSystem.h>

#include <fstream>
#include <iostream>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

namespace {

bool Check(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAILED: " << message << '\n';
  }
  return condition;
}

std::string TestDirectory() {
  return "/tmp/cshine_reconstructed_spectrum_merge_test_" +
         std::to_string(static_cast<long long>(getpid()));
}

void WriteInput(const std::string& path,
                double energy_offset,
                bool omit_last_histogram = false) {
  TFile output(path.c_str(), "RECREATE");
  for (unsigned int crystal = 0; crystal < 15U; ++crystal) {
    const std::string suffix = std::to_string(crystal);
    TH1I all(("h_recon_" + suffix).c_str(), "", 1000, 0.0, 200.0);
    TH1I silent(("h_recon_veto_" + suffix).c_str(), "", 1000, 0.0,
                200.0);
    TH1I vetoed(("h_recon_vetoed_" + suffix).c_str(), "", 1000, 0.0,
                200.0);
    TH1I detector(("h_eDep_" + suffix).c_str(), "", 1000, 0.0, 100.0);
    all.Fill(energy_offset + crystal);
    silent.Fill(energy_offset + crystal, 2.0);
    vetoed.Fill(energy_offset + crystal, 3.0);
    detector.Fill(energy_offset + crystal);
    detector.Write();
    all.Write();
    silent.Write();
    if (!omit_last_histogram || crystal != 14U) {
      vetoed.Write();
    }
  }
}

}  // namespace

int main() {
  bool ok = true;
  const std::string directory = TestDirectory();
  gSystem->mkdir(directory.c_str(), true);
  const std::string first = directory + "/run-a.root";
  const std::string second = directory + "/run-b.root";
  const std::string incomplete = directory + "/incomplete.root";
  const std::string per_crystal = directory + "/nested/all_notree.root";
  const std::string merged = directory + "/nested/all_recon.root";
  const std::string report = directory + "/nested/merge.tsv";
  WriteInput(first, 10.0);
  WriteInput(second, 20.0);
  WriteInput(incomplete, 30.0, true);

  const cshine_gamma::ReconstructedSpectrumMergeSummary summary =
      cshine_gamma::MergeReconstructedSpectra(
          cshine_gamma::HistoricalReconstructedSpectrumMergeDefinition(),
          {first, second}, per_crystal, merged, report, "beam-on", false);
  ok &= Check(summary.input_files.size() == 2U, "two explicit inputs");
  ok &= Check(summary.central_entries == 8.0,
              "central entries use four crystals and two runs");
  ok &= Check(summary.side_veto_silent_entries == 12.0,
              "side entries use six veto-silent crystal spectra and two runs");
  ok &= Check(summary.total_entries == 20.0,
              "total entries combine central and side spectra");

  TFile per_crystal_file(per_crystal.c_str(), "READ");
  for (unsigned int crystal = 0; crystal < 15U; ++crystal) {
    for (const char* prefix : {"h_eDep_", "h_recon_", "h_recon_veto_",
                               "h_recon_vetoed_"}) {
      TH1* histogram = nullptr;
      per_crystal_file.GetObject(
          (std::string(prefix) + std::to_string(crystal)).c_str(), histogram);
      ok &= Check(histogram != nullptr,
                  std::string("summed per-crystal object ") + prefix);
      if (histogram != nullptr) {
        ok &= Check(histogram->GetNbinsX() == 1000,
                    "per-crystal energy binning");
      }
    }
  }

  TFile merged_file(merged.c_str(), "READ");
  TH1* central = nullptr;
  TH1* side = nullptr;
  TH1* total = nullptr;
  TH1* rate = nullptr;
  TCanvas* canvas = nullptr;
  merged_file.GetObject("h_central_E_M1", central);
  merged_file.GetObject("h_side_E_M1", side);
  merged_file.GetObject("h_total_E_M1", total);
  merged_file.GetObject("h_rate", rate);
  merged_file.GetObject("c1", canvas);
  ok &= Check(central != nullptr && side != nullptr && total != nullptr,
              "historical merged spectrum objects");
  ok &= Check(rate != nullptr && rate->GetNbinsX() == 20,
              "historical side-to-central ratio object");
  ok &= Check(canvas != nullptr, "historical diagnostic canvas object");
  if (central != nullptr && side != nullptr && total != nullptr) {
    ok &= Check(central->GetNbinsX() == 1000 && side->GetNbinsX() == 1000 &&
                    total->GetNbinsX() == 1000,
                "raw merged histograms remain at 0.2 MeV binning");
    ok &= Check(central->GetEntries() == 8.0 && side->GetEntries() == 12.0 &&
                    total->GetEntries() == 20.0,
                "central side total entry accounting");
  }

  std::ifstream report_stream(report.c_str());
  const std::string report_text(
      (std::istreambuf_iterator<char>(report_stream)),
      std::istreambuf_iterator<char>());
  ok &= Check(report_text.find("config\tcentral_input\th_recon") !=
                  std::string::npos,
              "report records central source family");
  ok &= Check(report_text.find("config\tside_input\th_recon_veto") !=
                  std::string::npos,
              "report records side veto condition");

  bool duplicate_rejected = false;
  try {
    cshine_gamma::MergeReconstructedSpectra(
        cshine_gamma::HistoricalReconstructedSpectrumMergeDefinition(),
        {first, first}, directory + "/duplicate-notree.root",
        directory + "/duplicate-recon.root", directory + "/duplicate.tsv",
        "beam-off", false);
  } catch (const std::runtime_error&) {
    duplicate_rejected = true;
  }
  ok &= Check(duplicate_rejected, "duplicate input rejection");

  bool missing_object_rejected = false;
  try {
    cshine_gamma::MergeReconstructedSpectra(
        cshine_gamma::HistoricalReconstructedSpectrumMergeDefinition(),
        {incomplete}, directory + "/missing-notree.root",
        directory + "/missing-recon.root", directory + "/missing.tsv",
        "beam-off", false);
  } catch (const std::runtime_error&) {
    missing_object_rejected = true;
  }
  ok &= Check(missing_object_rejected, "missing input histogram rejection");

  bool overwrite_rejected = false;
  try {
    cshine_gamma::MergeReconstructedSpectra(
        cshine_gamma::HistoricalReconstructedSpectrumMergeDefinition(),
        {first}, per_crystal, merged, report, "beam-on", false);
  } catch (const std::runtime_error&) {
    overwrite_rejected = true;
  }
  ok &= Check(overwrite_rejected, "existing output rejection");
  return ok ? 0 : 1;
}
