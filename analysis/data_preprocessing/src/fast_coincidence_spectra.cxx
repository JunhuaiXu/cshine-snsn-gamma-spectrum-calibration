// Direct M11 migration of the fast/random-window selection in DP-S721--DP-S724.

#include "fast_coincidence_spectra.h"

#include "jiugong_recon.h"
#include "output_path_support.h"
#include "shower_reconstruction.h"

#include <TBranch.h>
#include <TChain.h>
#include <TChainElement.h>
#include <TFile.h>
#include <TH1I.h>
#include <TObjArray.h>
#include <TSystem.h>
#include <TTree.h>
#include <TTreeReader.h>
#include <TTreeReaderArray.h>
#include <TTreeReaderValue.h>

#include <array>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace cshine_gamma {
namespace {

struct SpectrumSet {
  TH1I central;
  TH1I side;
  TH1I total;

  SpectrumSet(const FastCoincidenceDefinition& definition)
      : central("h_central_E_M1", "", definition.energy_bins,
                definition.energy_min_mev, definition.energy_max_mev),
        side("h_side_E_M1", "", definition.energy_bins,
             definition.energy_min_mev, definition.energy_max_mev),
        total("h_total_E_M1", "", definition.energy_bins,
              definition.energy_min_mev, definition.energy_max_mev) {
    central.SetDirectory(nullptr);
    side.SetDirectory(nullptr);
    total.SetDirectory(nullptr);
  }
};

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

void RequireBranch(TTree& tree, const std::string& name) {
  if (tree.GetBranch(name.c_str()) == nullptr) {
    throw std::runtime_error("missing input branch: " + name);
  }
}

void ValidateTreeSchema(TTree& tree) {
  RequireBranch(tree, "TDC_Gamma_Trig_list");
  RequireBranch(tree, "GammaTime");
  RequireBranch(tree, "recon_result");
  RequireBranch(tree, "count_veto");
}

void ValidateEachInputFile(const FastCoincidenceDefinition& definition,
                           TChain& chain,
                           FastCoincidenceSummary& summary) {
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
    ValidateTreeSchema(*tree);
    summary.resolved_input_files.push_back(path);
  }
}

bool InInclusiveWindow(double value, double minimum, double maximum) {
  return value >= minimum && value <= maximum;
}

void FillCandidate(const jiugong_recon_result_t& result,
                   unsigned short veto_count,
                   SpectrumSet& spectra,
                   unsigned long long& central_count,
                   unsigned long long& side_count) {
  if (!IsMainSpectrumCandidate(result, veto_count)) {
    return;
  }
  const CrystalRole role =
      ClassifyCrystal(static_cast<unsigned short>(result.GetCenter()));
  if (role == CrystalRole::kCentral) {
    spectra.central.Fill(result.GetEnergy());
    ++central_count;
  } else {
    spectra.side.Fill(result.GetEnergy());
    ++side_count;
  }
  spectra.total.Fill(result.GetEnergy());
}

void WriteSpectrumFile(const std::string& path, SpectrumSet& spectra) {
  std::unique_ptr<TFile> output(TFile::Open(path.c_str(), "RECREATE"));
  if (!output || output->IsZombie()) {
    throw std::runtime_error("cannot create ROOT output: " + path);
  }
  output->cd();
  spectra.central.Write();
  spectra.side.Write();
  spectra.total.Write();
  output->Close();
}

void WriteReport(const FastCoincidenceDefinition& definition,
                 const FastCoincidenceSummary& summary,
                 const std::string& signal_output_root_file,
                 const std::string& random_output_root_file,
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
  report << "config\tinput_tree\t" << definition.input_tree_name << "\n";
  report << "config\tssd_m2_exclusion\tindex=" << definition.trigger_index
         << "\tstrict_" << definition.trigger_min_exclusive << ':'
         << definition.trigger_max_exclusive << "\n";
  report << "config\tsignal_time_window_ns\tinclusive\t"
         << definition.signal_time_min_ns << ':'
         << definition.signal_time_max_ns << "\n";
  report << "config\trandom_time_window_ns\tinclusive\t"
         << definition.random_time_min_ns << ':'
         << definition.random_time_max_ns << "\n";
  report << "config\tmain_candidate_policy\tcentral_no_veto_requirement\t"
            "side_requires_count_veto_zero\n";
  report << "config\tenergy_binning\t" << definition.energy_bins << '\t'
         << definition.energy_min_mev << ':' << definition.energy_max_mev
         << "\n";
  for (std::size_t index = 0; index < summary.input_specs.size(); ++index) {
    report << "input_spec\t" << summary.input_specs[index] << '\t'
           << summary.files_added_per_spec[index] << "\n";
  }
  for (std::size_t index = 0; index < summary.resolved_input_files.size();
       ++index) {
    report << "input_file\t" << index << '\t'
           << summary.resolved_input_files[index] << "\n";
  }
  report << "summary\tinput_entries\t" << summary.input_entries << "\n";
  report << "summary\texcluded_ssd_m2_entries\t"
         << summary.excluded_ssd_m2_entries << "\n";
  report << "summary\tsignal_candidates\tcentral="
         << summary.signal_central_candidates << "\tside="
         << summary.signal_side_candidates << "\n";
  report << "summary\trandom_candidates\tcentral="
         << summary.random_central_candidates << "\tside="
         << summary.random_side_candidates << "\n";
  report << "output\tsignal\t" << signal_output_root_file
         << "\th_central_E_M1,h_side_E_M1,h_total_E_M1\n";
  report << "output\trandom\t" << random_output_root_file
         << "\th_central_E_M1,h_side_E_M1,h_total_E_M1\n";
}

}  // namespace

FastCoincidenceDefinition HistoricalFastCoincidenceDefinition() {
  FastCoincidenceDefinition definition;
  definition.name = "remove_ssd_m2_equal_width_time_windows";
  definition.input_tree_name = "GammaCaliData";
  definition.trigger_index = 18U;
  definition.trigger_min_exclusive = 835U;
  definition.trigger_max_exclusive = 850U;
  definition.signal_time_min_ns = -350.0;
  definition.signal_time_max_ns = -50.0;
  definition.random_time_min_ns = 50.0;
  definition.random_time_max_ns = 350.0;
  definition.energy_bins = 1000;
  definition.energy_min_mev = 0.0;
  definition.energy_max_mev = 200.0;
  return definition;
}

void ValidateFastCoincidenceDefinition(
    const FastCoincidenceDefinition& definition) {
  const FastCoincidenceDefinition historical =
      HistoricalFastCoincidenceDefinition();
  if (definition.name != historical.name ||
      definition.input_tree_name != historical.input_tree_name ||
      definition.trigger_index != historical.trigger_index ||
      definition.trigger_min_exclusive !=
          historical.trigger_min_exclusive ||
      definition.trigger_max_exclusive !=
          historical.trigger_max_exclusive ||
      definition.signal_time_min_ns != historical.signal_time_min_ns ||
      definition.signal_time_max_ns != historical.signal_time_max_ns ||
      definition.random_time_min_ns != historical.random_time_min_ns ||
      definition.random_time_max_ns != historical.random_time_max_ns ||
      definition.energy_bins != historical.energy_bins ||
      definition.energy_min_mev != historical.energy_min_mev ||
      definition.energy_max_mev != historical.energy_max_mev) {
    throw std::invalid_argument("historical fast-coincidence definition changed");
  }
  ValidateShowerReconstructionDefinition(
      HistoricalShowerReconstructionDefinition());
}

FastCoincidenceSummary BuildFastCoincidenceSpectra(
    const FastCoincidenceDefinition& definition,
    const std::vector<std::string>& input_files_or_patterns,
    const std::string& signal_output_root_file,
    const std::string& random_output_root_file,
    const std::string& report_file,
    bool overwrite) {
  ValidateFastCoincidenceDefinition(definition);
  if (input_files_or_patterns.empty()) {
    throw std::invalid_argument("at least one input file or pattern is required");
  }
  if (signal_output_root_file == random_output_root_file) {
    throw std::invalid_argument("signal and random outputs must differ");
  }
  if (!report_file.empty() &&
      (report_file == signal_output_root_file ||
       report_file == random_output_root_file)) {
    throw std::invalid_argument("ROOT outputs and report paths must differ");
  }
  RequireNewOutputPath(signal_output_root_file, overwrite);
  RequireNewOutputPath(random_output_root_file, overwrite);
  if (!report_file.empty()) {
    RequireNewOutputPath(report_file, overwrite);
  }

  TChain chain(definition.input_tree_name.c_str(), "M8 reconstructed events");
  FastCoincidenceSummary summary = {};
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

  TTreeReader reader(&chain);
  TTreeReaderArray<UShort_t> trigger(reader, "TDC_Gamma_Trig_list");
  TTreeReaderArray<Double_t> gamma_time(reader, "GammaTime");
  TTreeReaderValue<std::map<unsigned short, jiugong_recon_result_t> >
      reconstructions(reader, "recon_result");
  TTreeReaderValue<UShort_t> veto_count(reader, "count_veto");
  SpectrumSet signal(definition);
  SpectrumSet random(definition);

  while (reader.Next()) {
    ++summary.input_entries;
    if (trigger.GetSize() <= definition.trigger_index) {
      throw std::runtime_error("TDC_Gamma_Trig_list is shorter than 19 values");
    }
    if (gamma_time.GetSize() != 15U) {
      throw std::runtime_error("GammaTime does not contain 15 crystal values");
    }
    const unsigned short trigger_value = trigger[definition.trigger_index];
    if (trigger_value > definition.trigger_min_exclusive &&
        trigger_value < definition.trigger_max_exclusive) {
      ++summary.excluded_ssd_m2_entries;
      continue;
    }
    for (std::map<unsigned short, jiugong_recon_result_t>::const_iterator item =
             reconstructions->begin();
         item != reconstructions->end(); ++item) {
      const jiugong_recon_result_t& result = item->second;
      if (!IsValidReconstruction(result)) {
        continue;
      }
      const int core = result.GetCenter();
      if (core < 0 || core >= static_cast<int>(gamma_time.GetSize())) {
        throw std::runtime_error("reconstructed core is outside GammaTime");
      }
      const double time = gamma_time[static_cast<std::size_t>(core)];
      if (InInclusiveWindow(time, definition.signal_time_min_ns,
                            definition.signal_time_max_ns)) {
        FillCandidate(result, *veto_count, signal,
                      summary.signal_central_candidates,
                      summary.signal_side_candidates);
      }
      if (InInclusiveWindow(time, definition.random_time_min_ns,
                            definition.random_time_max_ns)) {
        FillCandidate(result, *veto_count, random,
                      summary.random_central_candidates,
                      summary.random_side_candidates);
      }
    }
  }

  detail::EnsureOutputParentDirectories(std::vector<std::string>{
      signal_output_root_file, random_output_root_file, report_file});
  WriteSpectrumFile(signal_output_root_file, signal);
  WriteSpectrumFile(random_output_root_file, random);
  WriteReport(definition, summary, signal_output_root_file,
              random_output_root_file, report_file);
  return summary;
}

}  // namespace cshine_gamma
