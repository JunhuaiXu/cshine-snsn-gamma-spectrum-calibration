// Provenance: DP-S500--DP-S503, central 0308 PreRun calibrated-event producer.

#include "calibrated_event_tree.h"

#include "gamma_raw_data.hpp"
#include "output_path_support.h"
#include "t_gamma_cali.h"
#include "time_calibration.h"

#include <TBranch.h>
#include <TChain.h>
#include <TChainElement.h>
#include <TFile.h>
#include <TLeaf.h>
#include <TObjArray.h>
#include <TSystem.h>
#include <TTree.h>
#include <TTreeReader.h>
#include <TTreeReaderArray.h>
#include <TTreeReaderValue.h>

#include <cmath>
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

void RequireIntBranch(TTree& tree, const std::string& branch_name) {
  TBranch* branch = tree.GetBranch(branch_name.c_str());
  if (branch == nullptr) {
    throw std::runtime_error("missing input branch: " + branch_name);
  }
  TLeaf* leaf = branch->GetLeaf(branch_name.c_str());
  if (leaf == nullptr) {
    throw std::runtime_error("missing input leaf: " + branch_name);
  }
  if (std::string(leaf->GetTypeName()) != "Int_t") {
    throw std::runtime_error("input branch is not Int_t: " + branch_name);
  }
}

void RequireBranch(TTree& tree, const std::string& branch_name) {
  if (tree.GetBranch(branch_name.c_str()) == nullptr) {
    throw std::runtime_error("missing input branch: " + branch_name);
  }
}

void ValidateRawTreeSchema(TTree& tree,
                           const CalibratedEventDefinition& definition) {
  for (unsigned int crystal = 0; crystal < definition.crystal_count;
       ++crystal) {
    RequireIntBranch(tree, RawLowGainBranchName(crystal));
    RequireIntBranch(tree, RawHighGainBranchName(crystal));
    RequireIntBranch(tree, RawGammaTdcBranchName(crystal));
  }
  for (unsigned int t0 = 0; t0 < definition.t0_count; ++t0) {
    RequireIntBranch(tree, RawT0TdcBranchName(t0));
  }
  for (unsigned int veto = 0; veto < definition.veto_count; ++veto) {
    RequireIntBranch(tree, RawVetoAdcBranchName(veto));
    RequireIntBranch(tree, RawVetoTdcBranchName(veto));
  }
  RequireBranch(tree, RawGammaTriggerTdcBranchName());
}

void ValidateEachInputFile(const CalibratedEventDefinition& definition,
                           TChain& chain,
                           CalibratedEventSummary& summary) {
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
    ValidateRawTreeSchema(*tree, definition);
    summary.resolved_input_files.push_back(path);
  }
}

void CountEnergyPath(const t_gamma_cali& calibration,
                     unsigned int crystal,
                     unsigned short low_gain,
                     unsigned short high_gain,
                     CalibratedEventChannelSummary& summary) {
  if (high_gain > 4000U) {
    ++summary.saturated_high_gain_count;
    ++summary.low_gain_count;
    return;
  }
  const double converted_high_gain = calibration.GetXE(crystal, low_gain);
  if (converted_high_gain < 3000.0) {
    ++summary.high_gain_count;
  } else if (converted_high_gain < 3500.0) {
    ++summary.blended_gain_count;
  } else {
    ++summary.low_gain_count;
  }
}

void WriteRunReport(const CalibratedEventDefinition& definition,
                    const CalibratedEventSummary& summary,
                    const std::string& calibration_root_file,
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
  report << "config\tinput_tree\t" << definition.input_tree_name
         << "\t\t\n";
  report << "config\toutput_tree\t" << definition.output_tree_name
         << "\t\t\n";
  report << "config\tcalibration_object\t"
         << definition.calibration_object_name << "\t\t\n";
  report << "config\tretained_trigger_tdc\tTDC_Gamma_Trig\t"
            "TDC_Gamma_Trig_list[32]\t\n";
  report << "input\tcalibration\t" << calibration_root_file << "\t"
         << definition.calibration_object_name << "\n";
  for (std::size_t index = 0; index < summary.input_specs.size(); ++index) {
    report << "input_spec\t" << summary.input_specs[index] << "\t"
           << summary.files_added_per_spec[index] << "\t\n";
  }
  for (std::size_t index = 0; index < summary.resolved_input_files.size();
       ++index) {
    report << "input_file\t" << index << "\t"
           << summary.resolved_input_files[index] << "\t\n";
  }
  report << "output\troot\t" << output_root_file << "\t"
         << definition.output_tree_name << "\n";
  report << "summary\tinput_entries\t" << summary.input_entries
         << "\t\t\n";
  report << "summary\toutput_entries\t" << summary.output_entries
         << "\t\t\n";
  for (unsigned int crystal = 0; crystal < definition.crystal_count;
       ++crystal) {
    const CalibratedEventChannelSummary& channel = summary.channels[crystal];
    report << "channel\t" << crystal << "\t" << channel.invalid_time_count
           << "\t" << channel.high_gain_count << ','
           << channel.blended_gain_count << ',' << channel.low_gain_count
           << "\t" << channel.saturated_high_gain_count << '\n';
  }
}

}  // namespace

CalibratedEventDefinition Central0308CalibratedEventDefinition() {
  CalibratedEventDefinition definition;
  definition.name = "central_0308_calibrated_event_tree";
  definition.input_tree_name = "tree";
  definition.output_tree_name = "GammaCaliData";
  definition.output_tree_title = "Gamma Calibrated Data";
  definition.calibration_object_name = "cali_20240308";
  definition.crystal_count = GammaCrystalCount();
  definition.adc_storage_count = GammaAdcStorageCount();
  definition.tdc_storage_count = GammaTdcStorageCount();
  definition.t0_count = T0ChannelCount();
  definition.veto_count = VetoChannelCount();
  return definition;
}

void ValidateCalibratedEventDefinition(
    const CalibratedEventDefinition& definition) {
  if (definition.name.empty() || definition.input_tree_name.empty() ||
      definition.output_tree_name.empty() ||
      definition.output_tree_title.empty() ||
      definition.calibration_object_name.empty()) {
    throw std::invalid_argument("calibrated-event names must not be empty");
  }
  if (definition.crystal_count != 15U ||
      definition.adc_storage_count != 32U ||
      definition.tdc_storage_count != 32U || definition.t0_count != 4U) {
    throw std::invalid_argument("historical calibrated-event dimensions changed");
  }
  if (definition.veto_count != 3U) {
    throw std::invalid_argument("historical veto-channel count changed");
  }
}

CalibratedEventSummary BuildCalibratedEventTree(
    const CalibratedEventDefinition& definition,
    const std::vector<std::string>& input_files_or_patterns,
    const std::string& calibration_root_file,
    const std::string& output_root_file,
    const std::string& report_file,
    bool overwrite) {
  ValidateCalibratedEventDefinition(definition);
  if (input_files_or_patterns.empty()) {
    throw std::invalid_argument("at least one input file or pattern is required");
  }
  if (calibration_root_file.empty()) {
    throw std::invalid_argument("calibration ROOT file must not be empty");
  }
  if (output_root_file == calibration_root_file ||
      (!report_file.empty() &&
       (report_file == output_root_file ||
        report_file == calibration_root_file))) {
    throw std::invalid_argument("input, ROOT output, and report paths must differ");
  }
  RequireNewOutputPath(output_root_file, overwrite);
  if (!report_file.empty()) {
    RequireNewOutputPath(report_file, overwrite);
  }

  std::unique_ptr<TFile> calibration_file(
      TFile::Open(calibration_root_file.c_str(), "READ"));
  if (!calibration_file || calibration_file->IsZombie()) {
    throw std::runtime_error("cannot open calibration ROOT file: " +
                             calibration_root_file);
  }
  t_gamma_cali* calibration = nullptr;
  calibration_file->GetObject(definition.calibration_object_name.c_str(),
                              calibration);
  if (calibration == nullptr) {
    throw std::runtime_error("missing calibration object: " +
                             definition.calibration_object_name);
  }

  TChain chain(definition.input_tree_name.c_str(), "raw detector events");
  CalibratedEventSummary summary = {};
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

  detail::EnsureOutputParentDirectories(
      std::vector<std::string>{output_root_file, report_file});
  std::unique_ptr<TFile> output(TFile::Open(output_root_file.c_str(), "RECREATE"));
  if (!output || output->IsZombie()) {
    throw std::runtime_error("cannot create ROOT output: " + output_root_file);
  }
  TTree output_tree(definition.output_tree_name.c_str(),
                    definition.output_tree_title.c_str());
  std::array<Double_t, 15> gamma_energy = {};
  std::array<Double_t, 15> gamma_time = {};
  std::array<UShort_t, 32> adc_gamma = {};
  std::array<UShort_t, 32> tdc_gamma = {};
  std::array<UShort_t, 32> tdc_gamma_trigger = {};
  std::array<UShort_t, 4> tdc_t0 = {};
  std::array<Int_t, 3> adc_veto = {};
  std::array<Int_t, 3> tdc_veto = {};
  output_tree.Branch("GammaEnergy", &gamma_energy);
  output_tree.Branch("GammaTime", &gamma_time);
  output_tree.Branch("ADC_Gamma", &adc_gamma);
  output_tree.Branch("TDC_Gamma", &tdc_gamma);
  output_tree.Branch("TDC_Gamma_Trig_list", &tdc_gamma_trigger);
  output_tree.Branch("TDC_T0", &tdc_t0);
  output_tree.Branch("ADC_Veto", &adc_veto);
  output_tree.Branch("TDC_Veto", &tdc_veto);

  TTreeReader reader(&chain);
  std::vector<std::unique_ptr<TTreeReaderValue<Int_t> > > low_gain;
  std::vector<std::unique_ptr<TTreeReaderValue<Int_t> > > high_gain;
  std::vector<std::unique_ptr<TTreeReaderValue<Int_t> > > gamma_tdc;
  std::vector<std::unique_ptr<TTreeReaderValue<Int_t> > > t0_tdc;
  std::vector<std::unique_ptr<TTreeReaderValue<Int_t> > > veto_adc;
  std::vector<std::unique_ptr<TTreeReaderValue<Int_t> > > veto_tdc;
  TTreeReaderArray<UShort_t> trigger_tdc(
      reader, RawGammaTriggerTdcBranchName());
  for (unsigned int crystal = 0; crystal < definition.crystal_count;
       ++crystal) {
    low_gain.push_back(std::unique_ptr<TTreeReaderValue<Int_t> >(
        new TTreeReaderValue<Int_t>(reader,
                                    RawLowGainBranchName(crystal).c_str())));
    high_gain.push_back(std::unique_ptr<TTreeReaderValue<Int_t> >(
        new TTreeReaderValue<Int_t>(reader,
                                    RawHighGainBranchName(crystal).c_str())));
    gamma_tdc.push_back(std::unique_ptr<TTreeReaderValue<Int_t> >(
        new TTreeReaderValue<Int_t>(reader,
                                    RawGammaTdcBranchName(crystal).c_str())));
  }
  for (unsigned int t0 = 0; t0 < definition.t0_count; ++t0) {
    t0_tdc.push_back(std::unique_ptr<TTreeReaderValue<Int_t> >(
        new TTreeReaderValue<Int_t>(reader, RawT0TdcBranchName(t0).c_str())));
  }
  for (unsigned int veto = 0; veto < definition.veto_count; ++veto) {
    veto_adc.push_back(std::unique_ptr<TTreeReaderValue<Int_t> >(
        new TTreeReaderValue<Int_t>(reader,
                                    RawVetoAdcBranchName(veto).c_str())));
    veto_tdc.push_back(std::unique_ptr<TTreeReaderValue<Int_t> >(
        new TTreeReaderValue<Int_t>(reader,
                                    RawVetoTdcBranchName(veto).c_str())));
  }

  while (reader.Next()) {
    adc_gamma.fill(0U);
    tdc_gamma.fill(0U);
    tdc_gamma_trigger.fill(0U);
    tdc_t0.fill(0U);
    adc_veto.fill(0);
    tdc_veto.fill(0);
    for (unsigned int crystal = 0; crystal < definition.crystal_count;
         ++crystal) {
      adc_gamma[crystal] = static_cast<UShort_t>(**low_gain[crystal]);
      adc_gamma[crystal + 16U] =
          static_cast<UShort_t>(**high_gain[crystal]);
      tdc_gamma[crystal] = static_cast<UShort_t>(**gamma_tdc[crystal]);
      gamma_energy[crystal] = calibration->GetEnergy(
          static_cast<UShort_t>(crystal), adc_gamma[crystal],
          adc_gamma[crystal + 16U]);
      gamma_time[crystal] = CorrectedGammaTimeOrNaNNs(
          crystal, tdc_gamma[crystal], adc_gamma[crystal]);
      if (std::isnan(gamma_time[crystal])) {
        ++summary.channels[crystal].invalid_time_count;
      }
      CountEnergyPath(*calibration, crystal, adc_gamma[crystal],
                      adc_gamma[crystal + 16U], summary.channels[crystal]);
    }
    for (unsigned int t0 = 0; t0 < definition.t0_count; ++t0) {
      tdc_t0[t0] = static_cast<UShort_t>(**t0_tdc[t0]);
    }
    for (unsigned int veto = 0; veto < definition.veto_count; ++veto) {
      adc_veto[veto] = **veto_adc[veto];
      tdc_veto[veto] = **veto_tdc[veto];
    }
    if (trigger_tdc.GetSize() != GammaTriggerTdcStorageCount()) {
      throw std::runtime_error("TDC_Gamma_Trig must contain 32 values");
    }
    for (unsigned int channel = 0;
         channel < GammaTriggerTdcStorageCount(); ++channel) {
      tdc_gamma_trigger[channel] = trigger_tdc[channel];
    }
    output_tree.Fill();
  }

  summary.input_entries = static_cast<unsigned long long>(chain.GetEntries());
  summary.output_entries =
      static_cast<unsigned long long>(output_tree.GetEntries());
  output->cd();
  output_tree.Write();
  output->Close();
  WriteRunReport(definition, summary, calibration_root_file, output_root_file,
                 report_file);
  return summary;
}

}  // namespace cshine_gamma
