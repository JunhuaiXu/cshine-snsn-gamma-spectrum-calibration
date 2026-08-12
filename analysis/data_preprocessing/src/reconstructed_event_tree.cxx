// Direct migration of the reconstruction and veto portion of DP-S600.

#include "reconstructed_event_tree.h"

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

#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <memory>
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

void RequireBranch(TTree& tree, const std::string& name) {
  if (tree.GetBranch(name.c_str()) == nullptr) {
    throw std::runtime_error("missing input branch: " + name);
  }
}

void ValidateTreeSchema(TTree& tree) {
  RequireBranch(tree, "GammaEnergy");
  RequireBranch(tree, "GammaTime");
  RequireBranch(tree, "ADC_Gamma");
  RequireBranch(tree, "TDC_Gamma");
  RequireBranch(tree, "TDC_Gamma_Trig_list");
  RequireBranch(tree, "ADC_Veto");
  RequireBranch(tree, "TDC_Veto");
  RequireBranch(tree, "TDC_T0");
}

void ValidateEachInputFile(const ReconstructedEventDefinition& definition,
                           TChain& chain,
                           ReconstructedEventSummary& summary) {
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

void RequireArraySize(std::size_t actual,
                      std::size_t expected,
                      const std::string& branch) {
  if (actual != expected) {
    throw std::runtime_error("unexpected array size for " + branch);
  }
}

void WriteReport(const ReconstructedEventDefinition& definition,
                 const ReconstructedEventSummary& summary,
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
  report << "config\tenergy_threshold_mev\t1\t\t\n";
  report << "config\tneighbor_time_ns\t50\t\t\n";
  report << "config\tseparate_core_time_ns\t100\t\t\n";
  report << "config\tveto_tdc\tstrict_100_lt_tdc_lt_4000\t3_faces\t\n";
  report << "config\tretained_trigger_tdc\tTDC_Gamma_Trig_list[32]\t"
            "unchanged_for_downstream_diagnostics\t\n";
  report << "config\tmain_cores\tcentral=5,6,9,10\tside=4,7,8,11,13,14\t\n";
  report << "config\treconstructed_histograms\t15_each\t"
            "h_recon,h_recon_veto,h_recon_vetoed\t\n";
  report << "config\tdetector_histograms\t15\th_eDep\t\n";
  for (std::size_t index = 0; index < summary.input_specs.size(); ++index) {
    report << "input_spec\t" << summary.input_specs[index] << '\t'
           << summary.files_added_per_spec[index] << "\t\n";
  }
  for (std::size_t index = 0; index < summary.resolved_input_files.size();
       ++index) {
    report << "input_file\t" << index << '\t'
           << summary.resolved_input_files[index] << "\t\n";
  }
  report << "output\troot\t" << output_root_file << '\t'
         << definition.output_tree_name << "\n";
  report << "output\troot_objects\t" << output_root_file << '\t'
         << "h_eDep_0..14,h_recon_0..14,h_recon_veto_0..14,"
            "h_recon_vetoed_0..14\n";
  report << "summary\tinput_entries\t" << summary.input_entries << "\t\t\n";
  report << "summary\toutput_entries\t" << summary.output_entries << "\t\t\n";
  report << "summary\tentries_without_reconstruction\t"
         << summary.entries_without_reconstruction << "\t\t\n";
  report << "summary\tvalid_reconstructions\t"
         << summary.valid_reconstruction_count << "\t\t\n";
  report << "summary\tplaceholder_reconstructions\t"
         << summary.placeholder_reconstruction_count << "\t\t\n";
  report << "summary\tmain_spectrum_candidates\t"
         << summary.main_spectrum_candidate_count << "\t\t\n";
  report << "summary\tside_candidates_rejected_by_veto\t"
         << summary.side_candidate_rejected_by_veto_count << "\t\t\n";
  for (unsigned int count = 0; count <= 3U; ++count) {
    report << "veto_multiplicity\t" << count << '\t'
           << summary.events_by_veto_signal_count[count] << "\t\n";
  }
}

}  // namespace

ReconstructedEventDefinition HistoricalReconstructedEventDefinition() {
  ReconstructedEventDefinition definition;
  definition.name = "central_0308_shower_reconstruction";
  definition.input_tree_name = "GammaCaliData";
  definition.output_tree_name = "GammaCaliData";
  definition.output_tree_title = "Gamma Calibrated Data";
  return definition;
}

void ValidateReconstructedEventDefinition(
    const ReconstructedEventDefinition& definition) {
  if (definition.name != "central_0308_shower_reconstruction" ||
      definition.input_tree_name != "GammaCaliData" ||
      definition.output_tree_name != "GammaCaliData" ||
      definition.output_tree_title != "Gamma Calibrated Data") {
    throw std::invalid_argument("historical reconstructed-event definition changed");
  }
  ValidateShowerReconstructionDefinition(
      HistoricalShowerReconstructionDefinition());
}

ReconstructedEventSummary BuildReconstructedEventTree(
    const ReconstructedEventDefinition& definition,
    const std::vector<std::string>& input_files_or_patterns,
    const std::string& output_root_file,
    const std::string& report_file,
    bool overwrite) {
  ValidateReconstructedEventDefinition(definition);
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
  ReconstructedEventSummary summary = {};
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

  std::array<std::unique_ptr<TH1I>, 15> reconstructed_histograms;
  std::array<std::unique_ptr<TH1I>, 15> detector_histograms;
  std::array<std::unique_ptr<TH1I>, 15> veto_silent_histograms;
  std::array<std::unique_ptr<TH1I>, 15> vetoed_histograms;
  for (unsigned int crystal = 0; crystal < 15U; ++crystal) {
    const std::string suffix = std::to_string(crystal);
    detector_histograms[crystal].reset(
        new TH1I(("h_eDep_" + suffix).c_str(), "", 1000, 0.0, 100.0));
    reconstructed_histograms[crystal].reset(
        new TH1I(("h_recon_" + suffix).c_str(), "", 1000, 0.0, 200.0));
    veto_silent_histograms[crystal].reset(
        new TH1I(("h_recon_veto_" + suffix).c_str(), "", 1000, 0.0,
                 200.0));
    vetoed_histograms[crystal].reset(
        new TH1I(("h_recon_vetoed_" + suffix).c_str(), "", 1000, 0.0,
                 200.0));
    detector_histograms[crystal]->SetDirectory(nullptr);
    reconstructed_histograms[crystal]->SetDirectory(nullptr);
    veto_silent_histograms[crystal]->SetDirectory(nullptr);
    vetoed_histograms[crystal]->SetDirectory(nullptr);
  }

  std::array<Double_t, 15> gamma_energy = {};
  std::array<Double_t, 15> gamma_time = {};
  std::array<UShort_t, 32> adc_gamma = {};
  std::array<UShort_t, 32> tdc_gamma = {};
  std::array<UShort_t, 32> tdc_gamma_trigger = {};
  std::array<Int_t, 3> adc_veto = {};
  std::array<Int_t, 3> tdc_veto = {};
  std::array<UShort_t, 4> tdc_t0 = {};
  std::map<unsigned short, jiugong_recon_result_t> recon_result;
  UShort_t count_veto = 0U;

  output_tree.Branch("GammaEnergy", &gamma_energy);
  output_tree.Branch("GammaTime", &gamma_time);
  output_tree.Branch("ADC_Gamma", &adc_gamma);
  output_tree.Branch("TDC_Gamma", &tdc_gamma);
  output_tree.Branch("TDC_Gamma_Trig_list", &tdc_gamma_trigger);
  output_tree.Branch("ADC_Veto", &adc_veto);
  output_tree.Branch("TDC_Veto", &tdc_veto);
  output_tree.Branch("TDC_T0", &tdc_t0);
  output_tree.Branch("recon_result", &recon_result);
  output_tree.Branch("count_veto", &count_veto, "count_veto/s");

  TTreeReader reader(&chain);
  TTreeReaderArray<Double_t> energy_reader(reader, "GammaEnergy");
  TTreeReaderArray<Double_t> time_reader(reader, "GammaTime");
  TTreeReaderArray<UShort_t> adc_reader(reader, "ADC_Gamma");
  TTreeReaderArray<UShort_t> tdc_reader(reader, "TDC_Gamma");
  TTreeReaderArray<UShort_t> trigger_tdc_reader(
      reader, "TDC_Gamma_Trig_list");
  TTreeReaderArray<Int_t> veto_adc_reader(reader, "ADC_Veto");
  TTreeReaderArray<Int_t> veto_tdc_reader(reader, "TDC_Veto");
  TTreeReaderArray<UShort_t> t0_reader(reader, "TDC_T0");

  while (reader.Next()) {
    RequireArraySize(energy_reader.GetSize(), 15U, "GammaEnergy");
    RequireArraySize(time_reader.GetSize(), 15U, "GammaTime");
    RequireArraySize(adc_reader.GetSize(), 32U, "ADC_Gamma");
    RequireArraySize(tdc_reader.GetSize(), 32U, "TDC_Gamma");
    RequireArraySize(trigger_tdc_reader.GetSize(), 32U,
                     "TDC_Gamma_Trig_list");
    RequireArraySize(veto_adc_reader.GetSize(), 3U, "ADC_Veto");
    RequireArraySize(veto_tdc_reader.GetSize(), 3U, "TDC_Veto");
    RequireArraySize(t0_reader.GetSize(), 4U, "TDC_T0");
    for (unsigned int crystal = 0; crystal < 15U; ++crystal) {
      gamma_energy[crystal] = energy_reader[crystal];
      gamma_time[crystal] = time_reader[crystal];
      if (!std::isnan(gamma_time[crystal])) {
        detector_histograms[crystal]->Fill(gamma_energy[crystal]);
      }
    }
    for (unsigned int channel = 0; channel < 32U; ++channel) {
      adc_gamma[channel] = adc_reader[channel];
      tdc_gamma[channel] = tdc_reader[channel];
      tdc_gamma_trigger[channel] = trigger_tdc_reader[channel];
    }
    for (unsigned int veto = 0; veto < 3U; ++veto) {
      adc_veto[veto] = veto_adc_reader[veto];
      tdc_veto[veto] = veto_tdc_reader[veto];
    }
    for (unsigned int t0 = 0; t0 < 4U; ++t0) {
      tdc_t0[t0] = t0_reader[t0];
    }

    recon_result = jiugong_recon(gamma_energy, gamma_time);
    count_veto = CountVetoSignals(tdc_veto);
    ++summary.events_by_veto_signal_count[count_veto];
    if (recon_result.empty()) {
      ++summary.entries_without_reconstruction;
    }
    for (std::map<unsigned short, jiugong_recon_result_t>::const_iterator item =
             recon_result.begin();
         item != recon_result.end(); ++item) {
      const unsigned short crystal = item->first;
      if (crystal >= 15U) {
        throw std::runtime_error("reconstruction returned an invalid crystal index");
      }
      const double reconstructed_energy = item->second.GetEnergy();
      reconstructed_histograms[crystal]->Fill(reconstructed_energy);
      if (count_veto == 0U) {
        veto_silent_histograms[crystal]->Fill(reconstructed_energy);
      } else {
        vetoed_histograms[crystal]->Fill(reconstructed_energy);
      }
      if (!IsValidReconstruction(item->second)) {
        ++summary.placeholder_reconstruction_count;
        continue;
      }
      ++summary.valid_reconstruction_count;
      if (IsMainSpectrumCandidate(item->second, count_veto)) {
        ++summary.main_spectrum_candidate_count;
      } else if (ClassifyCrystal(
                     static_cast<unsigned short>(item->second.GetCenter())) ==
                     CrystalRole::kMainSide && count_veto > 0U) {
        ++summary.side_candidate_rejected_by_veto_count;
      }
    }
    output_tree.Fill();
  }

  summary.input_entries = static_cast<unsigned long long>(chain.GetEntries());
  summary.output_entries =
      static_cast<unsigned long long>(output_tree.GetEntries());
  output->cd();
  output_tree.Write();
  for (unsigned int crystal = 0; crystal < 15U; ++crystal) {
    detector_histograms[crystal]->Write();
    reconstructed_histograms[crystal]->Write();
    veto_silent_histograms[crystal]->Write();
    vetoed_histograms[crystal]->Write();
  }
  WriteReport(definition, summary, output_root_file, report_file);
  output->Close();
  return summary;
}

}  // namespace cshine_gamma
