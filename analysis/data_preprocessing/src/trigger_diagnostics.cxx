#include "trigger_diagnostics.h"

#include "jiugong_recon.h"
#include "output_path_support.h"
#include "shower_reconstruction.h"

#include <TChain.h>
#include <TChainElement.h>
#include <TFile.h>
#include <TH1I.h>
#include <TH2I.h>
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

namespace cshine_gamma {
namespace {

const char* const kConditionedLabels[5] = {
    "SSD M1 & CsI M1", "SSD M2", "SSD M1 & NA M1",
    "NA M1 & T0", "LS & T0"};

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

void RequireBranch(TTree& tree, const char* name) {
  if (tree.GetBranch(name) == nullptr) {
    throw std::runtime_error(std::string("missing input branch: ") + name);
  }
}

void ValidateInputFile(const TriggerDiagnosticsDefinition& definition,
                       const std::string& path) {
  std::unique_ptr<TFile> input(TFile::Open(path.c_str(), "READ"));
  if (!input || input->IsZombie()) {
    throw std::runtime_error("cannot open input ROOT file: " + path);
  }
  TTree* tree = nullptr;
  input->GetObject(definition.input_tree_name.c_str(), tree);
  if (tree == nullptr) {
    throw std::runtime_error("missing input tree: " + path);
  }
  RequireBranch(*tree, "TDC_Gamma_Trig_list");
  RequireBranch(*tree, "GammaTime");
  RequireBranch(*tree, "recon_result");
  RequireBranch(*tree, "count_veto");
}

bool MonitorIsValid(unsigned short value,
                    const TriggerDiagnosticsDefinition& definition) {
  return value > definition.monitor_min_exclusive &&
         value < definition.monitor_max_exclusive;
}

bool ConditionIsValid(unsigned short value,
                      const TriggerDiagnosticsDefinition& definition) {
  return value >= definition.condition_min_inclusive &&
         value <= definition.condition_max_inclusive;
}

bool IsHistoricalFigure15Candidate(const jiugong_recon_result_t& result,
                                   unsigned short veto_signal_count) {
  if (!IsValidReconstruction(result)) {
    return false;
  }
  const CrystalRole role =
      ClassifyCrystal(static_cast<unsigned short>(result.GetCenter()));
  if (role == CrystalRole::kCentral) {
    return veto_signal_count == 0U;
  }
  return role == CrystalRole::kMainSide;
}

std::string HistogramName(const char* policy, unsigned short trigger_index) {
  return std::string(policy) + "_h2_TOF_TotalE_Trig" +
         std::to_string(trigger_index);
}

void WriteReport(const TriggerDiagnosticsDefinition& definition,
                 const TriggerDiagnosticsSummary& summary,
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
  report << "config\tmonitor_window\tstrict_100_lt_tdc_lt_4000\t"
            "h1_TrigList0..14\t\n";
  report << "config\tcondition_window\tinclusive_100_le_tdc_le_4000\t"
            "indices=17,18,19,20,22\t\n";
  report << "config\thistorical_figure15_selection\t"
            "central_requires_count_veto=0\tside=no_veto_requirement\t"
            "frozen_macro_implementation\n";
  report << "config\treviewed_main_selection\tcentral=no_veto_requirement\t"
            "side_requires_count_veto=0\tauthor_confirmed\n";
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
         << "\ttrigger_diagnostics\n";
  report << "summary\tinput_entries\t" << summary.input_entries << "\t\t\n";
  for (unsigned int monitor = 0; monitor < 15U; ++monitor) {
    report << "monitor\t" << monitor << '\t' << monitor + 16U << '\t'
           << summary.monitor_entries[monitor] << "\n";
  }
  for (unsigned int condition = 0; condition < 5U; ++condition) {
    report << "condition\t" << definition.conditioned_indices[condition]
           << '\t' << kConditionedLabels[condition] << '\t'
           << summary.conditioned_event_entries[condition] << "\n";
    report << "historical_candidates\t"
           << definition.conditioned_indices[condition] << '\t'
           << summary.historical_candidate_entries[condition] << "\t\n";
    report << "reviewed_candidates\t"
           << definition.conditioned_indices[condition] << '\t'
           << summary.reviewed_candidate_entries[condition] << "\t\n";
  }
}

}  // namespace

TriggerDiagnosticsDefinition HistoricalTriggerDiagnosticsDefinition() {
  TriggerDiagnosticsDefinition definition;
  definition.name = "march_2024_trigger_diagnostics";
  definition.input_tree_name = "GammaCaliData";
  definition.monitor_min_exclusive = 100U;
  definition.monitor_max_exclusive = 4000U;
  definition.condition_min_inclusive = 100U;
  definition.condition_max_inclusive = 4000U;
  definition.conditioned_indices = {{17U, 18U, 19U, 20U, 22U}};
  return definition;
}

void ValidateTriggerDiagnosticsDefinition(
    const TriggerDiagnosticsDefinition& definition) {
  const TriggerDiagnosticsDefinition historical =
      HistoricalTriggerDiagnosticsDefinition();
  if (definition.name != historical.name ||
      definition.input_tree_name != historical.input_tree_name ||
      definition.monitor_min_exclusive != historical.monitor_min_exclusive ||
      definition.monitor_max_exclusive != historical.monitor_max_exclusive ||
      definition.condition_min_inclusive != historical.condition_min_inclusive ||
      definition.condition_max_inclusive != historical.condition_max_inclusive ||
      definition.conditioned_indices != historical.conditioned_indices) {
    throw std::invalid_argument("historical trigger definition changed");
  }
}

TriggerDiagnosticsSummary BuildTriggerDiagnostics(
    const TriggerDiagnosticsDefinition& definition,
    const std::vector<std::string>& input_files_or_patterns,
    const std::string& output_root_file,
    const std::string& report_file,
    bool overwrite) {
  ValidateTriggerDiagnosticsDefinition(definition);
  if (input_files_or_patterns.empty()) {
    throw std::invalid_argument("at least one input is required");
  }
  if (!report_file.empty() && report_file == output_root_file) {
    throw std::invalid_argument("ROOT output and report paths must differ");
  }
  RequireNewOutputPath(output_root_file, overwrite);
  if (!report_file.empty()) {
    RequireNewOutputPath(report_file, overwrite);
  }

  TChain chain(definition.input_tree_name.c_str());
  TriggerDiagnosticsSummary summary = {};
  summary.input_specs = input_files_or_patterns;
  for (std::vector<std::string>::const_iterator spec =
           input_files_or_patterns.begin();
       spec != input_files_or_patterns.end(); ++spec) {
    const int before = chain.GetListOfFiles()->GetEntries();
    chain.Add(spec->c_str());
    const int after = chain.GetListOfFiles()->GetEntries();
    const int added = after - before;
    if (added <= 0) {
      throw std::runtime_error("input matched no ROOT files: " + *spec);
    }
    summary.files_added_per_spec.push_back(added);
  }
  TObjArray* files = chain.GetListOfFiles();
  for (int index = 0; index < files->GetEntries(); ++index) {
    TChainElement* element = dynamic_cast<TChainElement*>(files->At(index));
    if (element == nullptr) {
      throw std::runtime_error("cannot resolve TChain input element");
    }
    const std::string path = element->GetTitle();
    ValidateInputFile(definition, path);
    summary.resolved_input_files.push_back(path);
  }
  if (chain.GetEntries() <= 0 || chain.LoadTree(0) < 0) {
    throw std::runtime_error("input chain contains no readable entries");
  }

  std::array<std::unique_ptr<TH1I>, 15> monitor_histograms;
  for (unsigned int monitor = 0; monitor < 15U; ++monitor) {
    const std::string name = "h1_TrigList" + std::to_string(monitor);
    monitor_histograms[monitor].reset(
        new TH1I(name.c_str(), "", 4096, 0.0, 4096.0));
    monitor_histograms[monitor]->SetDirectory(nullptr);
  }
  std::array<std::unique_ptr<TH2I>, 5> historical_histograms;
  std::array<std::unique_ptr<TH2I>, 5> reviewed_histograms;
  for (unsigned int condition = 0; condition < 5U; ++condition) {
    const unsigned short trigger_index =
        definition.conditioned_indices[condition];
    const std::string historical_name =
        HistogramName("historical", trigger_index);
    const std::string reviewed_name = HistogramName("reviewed", trigger_index);
    historical_histograms[condition].reset(new TH2I(
        historical_name.c_str(), "", 100, -500.0, 500.0, 200, 0.0, 200.0));
    reviewed_histograms[condition].reset(new TH2I(
        reviewed_name.c_str(), "", 100, -500.0, 500.0, 200, 0.0, 200.0));
    historical_histograms[condition]->SetDirectory(nullptr);
    reviewed_histograms[condition]->SetDirectory(nullptr);
  }

  TTreeReader reader(&chain);
  TTreeReaderArray<UShort_t> trigger_tdc(reader, "TDC_Gamma_Trig_list");
  TTreeReaderArray<Double_t> gamma_time(reader, "GammaTime");
  TTreeReaderValue<std::map<unsigned short, jiugong_recon_result_t> >
      reconstructions(reader, "recon_result");
  TTreeReaderValue<UShort_t> count_veto(reader, "count_veto");
  while (reader.Next()) {
    if (trigger_tdc.GetSize() != 32U || gamma_time.GetSize() != 15U) {
      throw std::runtime_error(
          "trigger TDC and GammaTime must contain 32 and 15 values");
    }
    for (unsigned int monitor = 0; monitor < 15U; ++monitor) {
      const unsigned short value = trigger_tdc[monitor + 16U];
      if (MonitorIsValid(value, definition)) {
        monitor_histograms[monitor]->Fill(value);
        ++summary.monitor_entries[monitor];
      }
    }
    for (unsigned int condition = 0; condition < 5U; ++condition) {
      const unsigned short trigger_index =
          definition.conditioned_indices[condition];
      if (!ConditionIsValid(trigger_tdc[trigger_index], definition)) {
        continue;
      }
      ++summary.conditioned_event_entries[condition];
      for (std::map<unsigned short, jiugong_recon_result_t>::const_iterator item =
               reconstructions->begin();
           item != reconstructions->end(); ++item) {
        if (!IsValidReconstruction(item->second)) {
          continue;
        }
        const unsigned short centre =
            static_cast<unsigned short>(item->second.GetCenter());
        const double total_energy = item->second.GetEnergy();
        if (IsHistoricalFigure15Candidate(item->second, *count_veto)) {
          historical_histograms[condition]->Fill(gamma_time[centre],
                                                  total_energy);
          ++summary.historical_candidate_entries[condition];
        }
        if (IsMainSpectrumCandidate(item->second, *count_veto)) {
          reviewed_histograms[condition]->Fill(gamma_time[centre], total_energy);
          ++summary.reviewed_candidate_entries[condition];
        }
      }
    }
  }
  summary.input_entries = static_cast<unsigned long long>(chain.GetEntries());

  detail::EnsureOutputParentDirectories(
      std::vector<std::string>{output_root_file, report_file});
  std::unique_ptr<TFile> output(TFile::Open(output_root_file.c_str(), "RECREATE"));
  if (!output || output->IsZombie()) {
    throw std::runtime_error("cannot create ROOT output: " + output_root_file);
  }
  output->cd();
  for (unsigned int monitor = 0; monitor < 15U; ++monitor) {
    monitor_histograms[monitor]->Write();
  }
  for (unsigned int condition = 0; condition < 5U; ++condition) {
    historical_histograms[condition]->Write();
    reviewed_histograms[condition]->Write();
  }
  WriteReport(definition, summary, output_root_file, report_file);
  output->Close();
  return summary;
}

}  // namespace cshine_gamma
