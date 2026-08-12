// Direct migration of DP-S504: neighboring-crystal timing diagnostics.

#include "neighbor_time_diagnostics.h"

#include "output_path_support.h"

#include <TBranch.h>
#include <TChain.h>
#include <TChainElement.h>
#include <TFile.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TObjArray.h>
#include <TSystem.h>
#include <TTree.h>

#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace cshine_gamma {
namespace {

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

void RequireBranch(TTree& tree, const std::string& branch_name) {
  if (tree.GetBranch(branch_name.c_str()) == nullptr) {
    throw std::runtime_error("missing input branch: " + branch_name);
  }
}

void ValidateEachInputFile(const NeighborTimeDefinition& definition,
                           TChain& chain,
                           NeighborTimeSummary& summary) {
  TObjArray* files = chain.GetListOfFiles();
  if (files == nullptr || files->GetEntries() == 0) {
    throw std::runtime_error("input specification matched no ROOT files");
  }
  for (int index = 0; index < files->GetEntries(); ++index) {
    TChainElement* element = dynamic_cast<TChainElement*>(files->At(index));
    if (element == nullptr) {
      throw std::runtime_error("cannot resolve a TChain input element");
    }
    const std::string path = element->GetTitle();
    std::unique_ptr<TFile> input(TFile::Open(path.c_str(), "READ"));
    if (!input || input->IsZombie()) {
      throw std::runtime_error("cannot open input ROOT file: " + path);
    }
    TTree* tree = nullptr;
    input->GetObject(definition.input_tree_name.c_str(), tree);
    if (tree == nullptr) {
      throw std::runtime_error("missing input tree " +
                               definition.input_tree_name + ": " + path);
    }
    RequireBranch(*tree, "GammaTime");
    RequireBranch(*tree, "GammaEnergy");
    summary.resolved_input_files.push_back(path);
  }
}

void RequireDrawSuccess(long long rows, const std::string& object_name) {
  if (rows < 0) {
    throw std::runtime_error("ROOT failed to fill object: " + object_name);
  }
}

void ValidateSelectedSubset(const TH2F& all, const TH2F& selected) {
  for (int x_bin = 0; x_bin <= all.GetNbinsX() + 1; ++x_bin) {
    for (int y_bin = 0; y_bin <= all.GetNbinsY() + 1; ++y_bin) {
      if (selected.GetBinContent(x_bin, y_bin) >
          all.GetBinContent(x_bin, y_bin)) {
        throw std::runtime_error(
            "energy-selected time-correlation bin exceeds no-cut bin");
      }
    }
  }
}

void WriteReport(const NeighborTimeDefinition& definition,
                 const NeighborTimeSummary& summary,
                 const TH2F& h2_all,
                 const TH2F& h2_cut,
                 const TH1F& h1,
                 const TH1F& hh_diff,
                 const TH1F& h3,
                 const TH1F& h4,
                 const std::string& output_root_file,
                 const std::string& report_file) {
  if (report_file.empty()) {
    return;
  }
  std::ofstream report(report_file.c_str());
  if (!report) {
    throw std::runtime_error("cannot create run report: " + report_file);
  }
  report << std::setprecision(17);
  report << "record\tfield_1\tfield_2\tfield_3\tfield_4\n";
  report << "config\tname\t" << definition.name << "\t\t\n";
  report << "config\tinput_tree\t" << definition.input_tree_name << "\t\t\n";
  report << "config\tcrystals\t" << definition.first_crystal << ','
         << definition.second_crystal << "\t\t\n";
  report << "config\tenergy_selection\tGammaEnergy["
         << definition.first_crystal << "]+GammaEnergy["
         << definition.second_crystal << "]>="
         << definition.energy_threshold_mev << "\tMeV\n";
  for (std::size_t index = 0; index < summary.input_specs.size(); ++index) {
    report << "input_spec\t" << summary.input_specs[index] << '\t'
           << summary.files_added_per_spec[index] << "\t\n";
  }
  for (std::size_t index = 0; index < summary.resolved_input_files.size();
       ++index) {
    report << "input_file\t" << index << '\t'
           << summary.resolved_input_files[index] << "\t\n";
  }
  report << "output\troot\t" << output_root_file
         << "\tneighbor-time diagnostics\n";
  report << "summary\ttree_entries\t" << summary.tree_entries << "\t\t\n";
  report << "summary\thistorical_peak_scale\t"
         << summary.historical_peak_scale << "\t\t\n";
  report << "object\th2_all\tTH2F\t" << summary.time_correlation_all_rows
         << '\t' << h2_all.Integral() << '\n';
  report << "object\th2_cut\tTH2F\t" << summary.time_correlation_cut_rows
         << '\t' << h2_cut.Integral() << '\n';
  report << "object\th1\tTH1F\t" << summary.time_difference_all_rows
         << '\t' << h1.Integral() << '\n';
  report << "object\thh_diff\tTH1F\t" << summary.time_difference_cut_rows
         << '\t' << hh_diff.Integral() << '\n';
  report << "object\th3\tTH1F\t" << summary.first_time_rows << '\t'
         << h3.Integral() << '\n';
  report << "object\th4\tTH1F\t" << summary.second_time_rows << '\t'
         << h4.Integral() << '\n';
}

}  // namespace

NeighborTimeDefinition HistoricalCsI05CsI06NeighborTimeDefinition() {
  NeighborTimeDefinition definition;
  definition.name = "historical_csi05_csi06_neighbor_time_diagnostics";
  definition.input_tree_name = "GammaCaliData";
  definition.first_crystal = 5U;
  definition.second_crystal = 6U;
  definition.energy_threshold_mev = 30.0;
  definition.time_bins = 100U;
  definition.time_min_ns = -500.0;
  definition.time_max_ns = 500.0;
  definition.difference_bins = 100U;
  definition.difference_min_ns = -200.0;
  definition.difference_max_ns = 200.0;
  return definition;
}

void ValidateNeighborTimeDefinition(const NeighborTimeDefinition& definition) {
  if (definition.name.empty() || definition.input_tree_name.empty()) {
    throw std::invalid_argument("neighbor-time names must not be empty");
  }
  if (definition.first_crystal >= 15U || definition.second_crystal >= 15U ||
      definition.first_crystal == definition.second_crystal) {
    throw std::invalid_argument("neighbor-time crystal indices are invalid");
  }
  if (definition.energy_threshold_mev != 30.0 ||
      definition.time_bins != 100U || definition.time_min_ns != -500.0 ||
      definition.time_max_ns != 500.0 ||
      definition.difference_bins != 100U ||
      definition.difference_min_ns != -200.0 ||
      definition.difference_max_ns != 200.0) {
    throw std::invalid_argument("historical neighbor-time definition changed");
  }
}

NeighborTimeSummary BuildNeighborTimeDiagnostics(
    const NeighborTimeDefinition& definition,
    const std::vector<std::string>& input_files_or_patterns,
    const std::string& output_root_file,
    const std::string& report_file,
    bool overwrite) {
  ValidateNeighborTimeDefinition(definition);
  if (input_files_or_patterns.empty()) {
    throw std::invalid_argument("at least one input file or pattern is required");
  }
  if (!report_file.empty() && report_file == output_root_file) {
    throw std::invalid_argument("ROOT output and report paths must differ");
  }
  RequireNewOutputPath(output_root_file, overwrite);
  if (!report_file.empty()) {
    RequireNewOutputPath(report_file, overwrite);
  }

  TChain chain(definition.input_tree_name.c_str(), "calibrated detector events");
  NeighborTimeSummary summary = {};
  summary.input_specs = input_files_or_patterns;
  for (std::vector<std::string>::const_iterator input =
           input_files_or_patterns.begin();
       input != input_files_or_patterns.end(); ++input) {
    const int before = chain.GetListOfFiles()->GetEntries();
    chain.Add(input->c_str());
    const int after = chain.GetListOfFiles()->GetEntries();
    const int added = after - before;
    if (added <= 0) {
      throw std::runtime_error("input matched no ROOT files: " + *input);
    }
    summary.files_added_per_spec.push_back(added);
  }
  ValidateEachInputFile(definition, chain, summary);
  if (chain.GetEntries() <= 0 || chain.LoadTree(0) < 0) {
    throw std::runtime_error("input chain contains no readable tree entries");
  }
  summary.tree_entries = static_cast<unsigned long long>(chain.GetEntries());

  detail::EnsureOutputParentDirectories(
      std::vector<std::string>{output_root_file, report_file});
  std::unique_ptr<TFile> output(TFile::Open(output_root_file.c_str(), "RECREATE"));
  if (!output || output->IsZombie()) {
    throw std::runtime_error("cannot create ROOT output: " + output_root_file);
  }
  output->cd();
  TH2F h2_all("h2_all", ";T_{5} [ns];T_{6} [ns]",
              definition.time_bins, definition.time_min_ns,
              definition.time_max_ns, definition.time_bins,
              definition.time_min_ns, definition.time_max_ns);
  TH2F h2_cut("h2_cut", ";T_{5} [ns];T_{6} [ns]",
              definition.time_bins, definition.time_min_ns,
              definition.time_max_ns, definition.time_bins,
              definition.time_min_ns, definition.time_max_ns);
  TH1F hh_diff("hh_diff", ";T_{5}-T_{6} [ns];Counts",
               definition.difference_bins, definition.difference_min_ns,
               definition.difference_max_ns);
  TH1F h1("h1", ";T_{5}-T_{6} [ns];Counts",
          definition.difference_bins, definition.difference_min_ns,
          definition.difference_max_ns);
  TH1F h3("h3", ";T_{5} [ns];Counts", definition.time_bins,
          definition.time_min_ns, definition.time_max_ns);
  TH1F h4("h4", ";T_{6} [ns];Counts", definition.time_bins,
          definition.time_min_ns, definition.time_max_ns);

  const std::string correlation = "GammaTime[6]:GammaTime[5]";
  const std::string difference = "GammaTime[5]-GammaTime[6]";
  const std::string selection = "GammaEnergy[5]+GammaEnergy[6]>=30";
  summary.time_correlation_all_rows =
      chain.Draw((correlation + ">>h2_all").c_str(), "", "goff");
  summary.time_correlation_cut_rows =
      chain.Draw((correlation + ">>h2_cut").c_str(), selection.c_str(), "goff");
  summary.time_difference_cut_rows =
      chain.Draw((difference + ">>hh_diff").c_str(), selection.c_str(), "goff");
  summary.time_difference_all_rows =
      chain.Draw((difference + ">>h1").c_str(), "", "goff");
  summary.first_time_rows = chain.Draw("GammaTime[5]>>h3", "", "goff");
  summary.second_time_rows = chain.Draw("GammaTime[6]>>h4", "", "goff");
  RequireDrawSuccess(summary.time_correlation_all_rows, "h2_all");
  RequireDrawSuccess(summary.time_correlation_cut_rows, "h2_cut");
  RequireDrawSuccess(summary.time_difference_cut_rows, "hh_diff");
  RequireDrawSuccess(summary.time_difference_all_rows, "h1");
  RequireDrawSuccess(summary.first_time_rows, "h3");
  RequireDrawSuccess(summary.second_time_rows, "h4");
  ValidateSelectedSubset(h2_all, h2_cut);
  summary.difference_all_peak = h1.GetMaximum();
  summary.difference_cut_peak = hh_diff.GetMaximum();
  if (summary.difference_cut_peak <= 0.0) {
    throw std::runtime_error("energy-selected time-difference histogram is empty");
  }
  summary.historical_peak_scale =
      summary.difference_all_peak / summary.difference_cut_peak;

  h2_all.Write();
  h2_cut.Write();
  hh_diff.Write();
  h1.Write();
  h3.Write();
  h4.Write();
  WriteReport(definition, summary, h2_all, h2_cut, h1, hh_diff, h3, h4,
              output_root_file, report_file);
  output->Close();
  return summary;
}

}  // namespace cshine_gamma
