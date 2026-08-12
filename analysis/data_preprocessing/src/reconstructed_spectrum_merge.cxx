// Direct M9 migration of DP-S602 and DP-S603 with explicit input files.

#include "reconstructed_spectrum_merge.h"

#include "output_path_support.h"

#include <TCanvas.h>
#include <TFile.h>
#include <TH1.h>
#include <TH1D.h>
#include <TH1I.h>
#include <TLegend.h>
#include <TROOT.h>
#include <TSystem.h>

#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <memory>
#include <set>
#include <stdexcept>

namespace cshine_gamma {
namespace {

const unsigned int kCrystalCount = 15U;

bool PathExists(const std::string& path) {
  return !gSystem->AccessPathName(path.c_str(), kFileExists);
}

void RequireNewOutputPath(const std::string& path, bool overwrite) {
  if (path.empty()) {
    throw std::invalid_argument("output path must not be empty");
  }
  if (!overwrite && PathExists(path)) {
    throw std::runtime_error("output already exists: " + path);
  }
}

std::string HistogramName(const std::string& prefix, unsigned int crystal) {
  return prefix + std::to_string(crystal);
}

std::unique_ptr<TH1I> NewEnergyHistogram(const std::string& name) {
  std::unique_ptr<TH1I> histogram(new TH1I(name.c_str(), "", 1000, 0.0, 200.0));
  histogram->SetDirectory(nullptr);
  return histogram;
}

void ValidateHistogram(const TH1& histogram,
                       const std::string& name,
                       double maximum_mev) {
  if (histogram.GetNbinsX() != 1000 ||
      std::abs(histogram.GetXaxis()->GetXmin()) > 1.0e-12 ||
      std::abs(histogram.GetXaxis()->GetXmax() - maximum_mev) > 1.0e-12) {
    throw std::runtime_error("unexpected histogram schema: " + name);
  }
}

TH1* RequireHistogram(TFile& input,
                      const std::string& name,
                      double maximum_mev) {
  TH1* histogram = nullptr;
  input.GetObject(name.c_str(), histogram);
  if (histogram == nullptr) {
    throw std::runtime_error("missing input histogram " + name + " in " +
                             input.GetName());
  }
  ValidateHistogram(*histogram, name, maximum_mev);
  return histogram;
}

void AddSelectedHistograms(
    TH1& target,
    const std::array<std::unique_ptr<TH1I>, kCrystalCount>& sources,
    const std::vector<unsigned short>& crystals) {
  for (std::vector<unsigned short>::const_iterator crystal = crystals.begin();
       crystal != crystals.end(); ++crystal) {
    target.Add(sources[*crystal].get());
  }
}

void WriteReport(const ReconstructedSpectrumMergeDefinition& definition,
                 const ReconstructedSpectrumMergeSummary& summary,
                 const std::string& per_crystal_output,
                 const std::string& merged_output,
                 const std::string& report_file) {
  if (report_file.empty()) {
    return;
  }
  std::ofstream report(report_file.c_str());
  if (!report) {
    throw std::runtime_error("cannot create run report: " + report_file);
  }
  report << std::setprecision(17);
  report << "record\tfield_1\tfield_2\tfield_3\n";
  report << "config\tname\t" << definition.name << "\n";
  report << "config\tsample_role\t" << summary.sample_role << "\n";
  report << "config\tcentral_crystals\t5,6,9,10\n";
  report << "config\tside_crystals\t4,7,8,11,13,14\n";
  report << "config\tcentral_input\th_recon\n";
  report << "config\tside_input\th_recon_veto\n";
  for (std::size_t index = 0; index < summary.input_files.size(); ++index) {
    report << "input_file\t" << index << '\t' << summary.input_files[index]
           << "\n";
  }
  report << "output\tper_crystal\t" << per_crystal_output << '\t'
         << "h_eDep_0..14,h_recon_0..14,h_recon_veto_0..14,"
            "h_recon_vetoed_0..14\n";
  report << "output\tmerged\t" << merged_output << '\t'
         << "h_central_E_M1,h_side_E_M1,h_total_E_M1,c1,h_rate\n";
  report << "summary\tcentral_entries\t" << summary.central_entries << "\n";
  report << "summary\tside_veto_silent_entries\t"
         << summary.side_veto_silent_entries << "\n";
  report << "summary\ttotal_entries\t" << summary.total_entries << "\n";
}

}  // namespace

ReconstructedSpectrumMergeDefinition
HistoricalReconstructedSpectrumMergeDefinition() {
  ReconstructedSpectrumMergeDefinition definition;
  definition.name = "central_0308_reconstructed_spectrum_merge";
  definition.crystal_count = 15U;
  definition.energy_bins = 1000;
  definition.energy_min_mev = 0.0;
  definition.energy_max_mev = 200.0;
  definition.central_crystals = {5, 6, 9, 10};
  definition.side_crystals = {4, 7, 8, 11, 13, 14};
  return definition;
}

void ValidateReconstructedSpectrumMergeDefinition(
    const ReconstructedSpectrumMergeDefinition& definition) {
  if (definition.name != "central_0308_reconstructed_spectrum_merge" ||
      definition.crystal_count != 15U || definition.energy_bins != 1000 ||
      definition.energy_min_mev != 0.0 ||
      definition.energy_max_mev != 200.0 ||
      definition.central_crystals !=
          std::vector<unsigned short>({5, 6, 9, 10}) ||
      definition.side_crystals !=
          std::vector<unsigned short>({4, 7, 8, 11, 13, 14})) {
    throw std::invalid_argument("historical reconstructed-spectrum definition changed");
  }
  std::set<unsigned short> used;
  used.insert(definition.central_crystals.begin(),
              definition.central_crystals.end());
  used.insert(definition.side_crystals.begin(), definition.side_crystals.end());
  if (used.size() != 10U || *used.rbegin() >= definition.crystal_count) {
    throw std::invalid_argument("invalid reconstructed-spectrum crystal groups");
  }
}

ReconstructedSpectrumMergeSummary MergeReconstructedSpectra(
    const ReconstructedSpectrumMergeDefinition& definition,
    const std::vector<std::string>& input_root_files,
    const std::string& per_crystal_output_root_file,
    const std::string& merged_output_root_file,
    const std::string& report_file,
    const std::string& sample_role,
    bool overwrite) {
  ValidateReconstructedSpectrumMergeDefinition(definition);
  if (input_root_files.empty()) {
    throw std::invalid_argument("at least one M8 input file is required");
  }
  if (sample_role != "beam-on" && sample_role != "beam-off") {
    throw std::invalid_argument("sample role must be beam-on or beam-off");
  }
  if (per_crystal_output_root_file == merged_output_root_file ||
      (!report_file.empty() &&
       (report_file == per_crystal_output_root_file ||
        report_file == merged_output_root_file))) {
    throw std::invalid_argument("M9 output paths must be distinct");
  }
  RequireNewOutputPath(per_crystal_output_root_file, overwrite);
  RequireNewOutputPath(merged_output_root_file, overwrite);
  if (!report_file.empty()) {
    RequireNewOutputPath(report_file, overwrite);
  }

  std::array<std::unique_ptr<TH1I>, kCrystalCount> reconstructed;
  std::array<std::unique_ptr<TH1I>, kCrystalCount> detector_energy;
  std::array<std::unique_ptr<TH1I>, kCrystalCount> veto_silent;
  std::array<std::unique_ptr<TH1I>, kCrystalCount> vetoed;
  for (unsigned int crystal = 0; crystal < kCrystalCount; ++crystal) {
    detector_energy[crystal].reset(new TH1I(
        HistogramName("h_eDep_", crystal).c_str(), "", 1000, 0.0, 100.0));
    detector_energy[crystal]->SetDirectory(nullptr);
    reconstructed[crystal] = NewEnergyHistogram(
        HistogramName("h_recon_", crystal));
    veto_silent[crystal] = NewEnergyHistogram(
        HistogramName("h_recon_veto_", crystal));
    vetoed[crystal] = NewEnergyHistogram(
        HistogramName("h_recon_vetoed_", crystal));
  }

  ReconstructedSpectrumMergeSummary summary = {};
  summary.sample_role = sample_role;
  std::set<std::string> unique_inputs;
  for (std::vector<std::string>::const_iterator input_path =
           input_root_files.begin();
       input_path != input_root_files.end(); ++input_path) {
    if (!unique_inputs.insert(*input_path).second) {
      throw std::runtime_error("duplicate M9 input file: " + *input_path);
    }
    std::unique_ptr<TFile> input(TFile::Open(input_path->c_str(), "READ"));
    if (!input || input->IsZombie()) {
      throw std::runtime_error("cannot open M9 input ROOT file: " + *input_path);
    }
    for (unsigned int crystal = 0; crystal < kCrystalCount; ++crystal) {
      detector_energy[crystal]->Add(RequireHistogram(
          *input, HistogramName("h_eDep_", crystal), 100.0));
      reconstructed[crystal]->Add(RequireHistogram(
          *input, HistogramName("h_recon_", crystal), 200.0));
      veto_silent[crystal]->Add(RequireHistogram(
          *input, HistogramName("h_recon_veto_", crystal), 200.0));
      vetoed[crystal]->Add(RequireHistogram(
          *input, HistogramName("h_recon_vetoed_", crystal), 200.0));
    }
    summary.input_files.push_back(*input_path);
  }

  detail::EnsureOutputParentDirectories(
      std::vector<std::string>{per_crystal_output_root_file,
                               merged_output_root_file, report_file});
  {
    std::unique_ptr<TFile> output(
        TFile::Open(per_crystal_output_root_file.c_str(), "RECREATE"));
    if (!output || output->IsZombie()) {
      throw std::runtime_error("cannot create per-crystal ROOT output: " +
                               per_crystal_output_root_file);
    }
    output->cd();
    for (unsigned int crystal = 0; crystal < kCrystalCount; ++crystal) {
      detector_energy[crystal]->Write();
      reconstructed[crystal]->Write();
      veto_silent[crystal]->Write();
      vetoed[crystal]->Write();
    }
    output->Close();
  }

  std::unique_ptr<TH1I> central = NewEnergyHistogram("h_central_E_M1");
  std::unique_ptr<TH1I> side = NewEnergyHistogram("h_side_E_M1");
  std::unique_ptr<TH1I> total = NewEnergyHistogram("h_total_E_M1");
  AddSelectedHistograms(*central, reconstructed, definition.central_crystals);
  AddSelectedHistograms(*side, veto_silent, definition.side_crystals);
  total->Add(central.get(), side.get());
  summary.central_entries = central->GetEntries();
  summary.side_veto_silent_entries = side->GetEntries();
  summary.total_entries = total->GetEntries();

  std::unique_ptr<TFile> merged(
      TFile::Open(merged_output_root_file.c_str(), "RECREATE"));
  if (!merged || merged->IsZombie()) {
    throw std::runtime_error("cannot create merged ROOT output: " +
                             merged_output_root_file);
  }
  merged->cd();
  central->Write();
  side->Write();
  total->Write();

  gROOT->SetBatch(kTRUE);
  std::unique_ptr<TH1> display_total(
      dynamic_cast<TH1*>(total->Clone("h_total_E_M1_display")));
  std::unique_ptr<TH1> display_central(
      dynamic_cast<TH1*>(central->Clone("h_central_E_M1_display")));
  std::unique_ptr<TH1> display_side(
      dynamic_cast<TH1*>(side->Clone("h_side_E_M1_display")));
  display_total->SetDirectory(nullptr);
  display_central->SetDirectory(nullptr);
  display_side->SetDirectory(nullptr);
  display_total->Rebin(5);
  display_total->GetXaxis()->SetRangeUser(0.0, 100.0);
  display_central->Rebin(5);
  display_central->SetLineColor(kRed);
  display_side->Rebin(5);
  display_side->SetLineColor(kCyan);
  TCanvas canvas("c1", "Histogram");
  canvas.SetLogy();
  display_total->Draw();
  display_central->Draw("SAME");
  display_side->Draw("SAME");
  TLegend legend(0.4, 0.4, 0.7, 0.9);
  legend.SetFillStyle(0);
  legend.SetLineWidth(0);
  legend.AddEntry(display_total.get(), "Total");
  legend.AddEntry(display_side.get(), "Side");
  legend.AddEntry(display_central.get(), "Central");
  legend.Draw("SAME");
  canvas.Write();

  std::unique_ptr<TH1> rate_central(
      dynamic_cast<TH1*>(central->Clone("h_central_E_M1_rate")));
  std::unique_ptr<TH1> rate_side(
      dynamic_cast<TH1*>(side->Clone("h_side_E_M1_rate")));
  rate_central->SetDirectory(nullptr);
  rate_side->SetDirectory(nullptr);
  rate_central->Rebin(50);
  rate_side->Rebin(50);
  rate_central->Sumw2();
  rate_side->Sumw2();
  TH1D rate("h_rate", "Side/Central", 20, 0.0, 200.0);
  rate.Divide(rate_side.get(), rate_central.get());
  rate.Write();
  merged->Close();

  WriteReport(definition, summary, per_crystal_output_root_file,
              merged_output_root_file, report_file);
  return summary;
}

}  // namespace cshine_gamma
