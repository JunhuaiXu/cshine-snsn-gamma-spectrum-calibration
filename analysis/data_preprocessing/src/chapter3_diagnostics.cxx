#include "chapter3_diagnostics.h"

#include "jiugong_recon.h"
#include "output_path_support.h"
#include "shower_reconstruction.h"
#include "spatial_spread.hpp"

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

void RequireBranch(TTree& tree, const char* branch) {
  if (tree.GetBranch(branch) == nullptr) {
    throw std::runtime_error(std::string("missing input branch: ") + branch);
  }
}

void ValidateInputFile(const Chapter3DiagnosticsDefinition& definition,
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
  RequireBranch(*tree, "GammaEnergy");
  RequireBranch(*tree, "GammaTime");
  RequireBranch(*tree, "recon_result");
  RequireBranch(*tree, "count_veto");
}

void WriteReport(const Chapter3DiagnosticsDefinition& definition,
                 const Chapter3DiagnosticsSummary& summary,
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
  report << "config\tsample_role\t" << summary.sample_role << "\t\t\n";
  report << "config\tinput_tree\t" << definition.input_tree_name << "\t\t\n";
  report << "config\tcrystal_pitch_cm\t" << definition.crystal_pitch_cm
         << "\t\t\n";
  report << "config\tmain_selection\tcentral=5,6,9,10\t"
            "side=4,7,8,11,13,14\tside_requires_count_veto=0\n";
  report << "config\tmultiplicity_selection\tall_15_centres\tno_veto\t"
         << "E_tot_gt_" << definition.high_energy_threshold_mev << "_MeV\n";
  report << "config\treconstruction_input\tstored_recon_result\t"
            "not_recomputed\t\n";
  report << "config\tcore_time_sign\t"
         << (summary.sample_role == "beam-on" ? "+GammaTime" : "-GammaTime")
         << "\thistorical_sample_role_convention\t\n";
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
         << "\tchapter3_diagnostics\n";
  report << "output\troot_objects\t" << output_root_file << '\t'
         << "ALL_h2_TotalE_DeltaY,ALL_h2_TotalE_Delta,"
            "ALL_h2_ax_ay_10_100,ALL_h2_ax_ay_100_inf,"
            "central_h2_TotalE_CenterE,side_h2_TotalE_CenterE,"
            "ALL_h2_TotalE_CenterE,ALL_h2_TOF_TotalE,"
            "all15_cluster_size_vs_total_energy,all15_core_multiplicity,"
            "all15_high_core_multiplicity\n";
  report << "summary\tinput_entries\t" << summary.input_entries << "\t\t\n";
  report << "summary\tvalid_reconstructions\t"
         << summary.valid_reconstruction_count << "\t\t\n";
  report << "summary\tplaceholder_reconstructions\t"
         << summary.placeholder_reconstruction_count << "\t\t\n";
  report << "summary\tmain_candidates\t" << summary.main_candidate_count
         << "\t\t\n";
  report << "summary\tcentral_candidates\t"
         << summary.central_candidate_count << "\t\t\n";
  report << "summary\tside_candidates\t" << summary.side_candidate_count
         << "\t\t\n";
  report << "summary\tside_rejected_by_veto\t"
         << summary.side_candidate_rejected_by_veto_count << "\t\t\n";
  report << "summary\tall15_candidates\t" << summary.all15_candidate_count
         << "\t\t\n";
  report << "summary\ttriggers_with_reconstruction\t"
         << summary.triggers_with_reconstruction << "\t\t\n";
  report << "summary\ttriggers_with_high_energy_reconstruction\t"
         << summary.triggers_with_high_energy_reconstruction << "\t\t\n";
}

}  // namespace

Chapter3DiagnosticsDefinition HistoricalChapter3DiagnosticsDefinition() {
  Chapter3DiagnosticsDefinition definition;
  definition.name = "central_chapter3_diagnostics";
  definition.input_tree_name = "GammaCaliData";
  definition.crystal_pitch_cm = 7.0;
  definition.high_energy_threshold_mev = 35.0;
  return definition;
}

void ValidateChapter3DiagnosticsDefinition(
    const Chapter3DiagnosticsDefinition& definition) {
  if (definition.name != "central_chapter3_diagnostics" ||
      definition.input_tree_name != "GammaCaliData" ||
      definition.crystal_pitch_cm != 7.0 ||
      definition.high_energy_threshold_mev != 35.0) {
    throw std::invalid_argument("historical Chapter 3 definition changed");
  }
}

Chapter3DiagnosticsSummary BuildChapter3Diagnostics(
    const Chapter3DiagnosticsDefinition& definition,
    const std::vector<std::string>& input_files_or_patterns,
    const std::string& output_root_file,
    const std::string& report_file,
    const std::string& sample_role,
    bool overwrite) {
  ValidateChapter3DiagnosticsDefinition(definition);
  if (input_files_or_patterns.empty()) {
    throw std::invalid_argument("at least one input is required");
  }
  if (sample_role != "beam-on" && sample_role != "beam-off") {
    throw std::invalid_argument("sample role must be beam-on or beam-off");
  }
  if (!report_file.empty() && report_file == output_root_file) {
    throw std::invalid_argument("ROOT output and report paths must differ");
  }
  RequireNewOutputPath(output_root_file, overwrite);
  if (!report_file.empty()) {
    RequireNewOutputPath(report_file, overwrite);
  }

  TChain chain(definition.input_tree_name.c_str());
  Chapter3DiagnosticsSummary summary = {};
  summary.sample_role = sample_role;
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

  TH2I total_energy_delta_y("ALL_h2_TotalE_DeltaY", "", 50, 5.0, 200.0,
                            70, 0.0, 7.0);
  TH2I total_energy_delta_r("ALL_h2_TotalE_Delta", "", 50, 0.0, 200.0,
                            70, 0.0, 7.0);
  TH2I delta_x_delta_y_low("ALL_h2_ax_ay_10_100", "", 70, 0.0, 7.0,
                           70, 0.0, 7.0);
  TH2I delta_x_delta_y_high("ALL_h2_ax_ay_100_inf", "", 70, 0.0, 7.0,
                            70, 0.0, 7.0);
  TH2I central_total_core("central_h2_TotalE_CenterE", "", 50, 5.0, 200.0,
                          80, 0.0, 80.0);
  TH2I side_total_core("side_h2_TotalE_CenterE", "", 50, 5.0, 200.0,
                       80, 0.0, 80.0);
  TH2I all_total_core("ALL_h2_TotalE_CenterE", "", 50, 5.0, 200.0,
                      80, 0.0, 80.0);
  TH2I core_time_total_energy("ALL_h2_TOF_TotalE", "", 100, -500.0, 500.0,
                              200, 0.0, 200.0);
  TH2I all15_cluster_size("all15_cluster_size_vs_total_energy", "", 200,
                          0.0, 200.0, 9, 0.5, 9.5);
  TH1I all15_multiplicity("all15_core_multiplicity", "", 9, 0.5, 9.5);
  TH1I all15_high_multiplicity("all15_high_core_multiplicity", "", 9, 0.5,
                               9.5);
  const std::vector<TH1*> objects = {
      &total_energy_delta_y, &total_energy_delta_r, &delta_x_delta_y_low,
      &delta_x_delta_y_high, &central_total_core, &side_total_core,
      &all_total_core, &core_time_total_energy, &all15_cluster_size,
      &all15_multiplicity, &all15_high_multiplicity};
  for (std::vector<TH1*>::const_iterator object = objects.begin();
       object != objects.end(); ++object) {
    (*object)->SetDirectory(nullptr);
  }

  TTreeReader reader(&chain);
  TTreeReaderArray<Double_t> gamma_energy(reader, "GammaEnergy");
  TTreeReaderArray<Double_t> gamma_time(reader, "GammaTime");
  TTreeReaderValue<std::map<unsigned short, jiugong_recon_result_t> >
      reconstructions(reader, "recon_result");
  TTreeReaderValue<UShort_t> count_veto(reader, "count_veto");
  while (reader.Next()) {
    if (gamma_energy.GetSize() != 15U || gamma_time.GetSize() != 15U) {
      throw std::runtime_error("GammaEnergy and GammaTime must have 15 values");
    }
    unsigned int all_count = 0U;
    unsigned int high_count = 0U;
    for (std::map<unsigned short, jiugong_recon_result_t>::const_iterator item =
             reconstructions->begin();
         item != reconstructions->end(); ++item) {
      if (!IsValidReconstruction(item->second)) {
        ++summary.placeholder_reconstruction_count;
        continue;
      }
      ++summary.valid_reconstruction_count;
      ++summary.all15_candidate_count;
      ++all_count;
      const double total_energy = item->second.GetEnergy();
      all15_cluster_size.Fill(total_energy, item->second.GetMultiplicity());
      if (total_energy > definition.high_energy_threshold_mev) {
        ++high_count;
      }

      const CrystalRole role = ClassifyCrystal(
          static_cast<unsigned short>(item->second.GetCenter()));
      if (role != CrystalRole::kCentral &&
          role != CrystalRole::kMainSide) {
        continue;
      }
      if (role == CrystalRole::kMainSide && *count_veto > 0U) {
        ++summary.side_candidate_rejected_by_veto_count;
        continue;
      }
      ++summary.main_candidate_count;
      const unsigned short centre =
          static_cast<unsigned short>(item->second.GetCenter());
      const SpatialSpread spread = CalculateSpatialSpread(item->second);
      const double delta_x = spread.delta_x_pitch * definition.crystal_pitch_cm;
      const double delta_y = spread.delta_y_pitch * definition.crystal_pitch_cm;
      const double delta_r = std::sqrt(delta_x * delta_x + delta_y * delta_y);
      total_energy_delta_y.Fill(total_energy, delta_y);
      total_energy_delta_r.Fill(total_energy, delta_r);
      all_total_core.Fill(total_energy, gamma_energy[centre]);
      core_time_total_energy.Fill(
          sample_role == "beam-on" ? gamma_time[centre] : -gamma_time[centre],
          total_energy);
      if (role == CrystalRole::kCentral) {
        ++summary.central_candidate_count;
        central_total_core.Fill(total_energy, gamma_energy[centre]);
      } else {
        ++summary.side_candidate_count;
        side_total_core.Fill(total_energy, gamma_energy[centre]);
      }
      if (total_energy >= 10.0 && total_energy <= 100.0) {
        delta_x_delta_y_low.Fill(delta_x, delta_y);
      } else if (total_energy > 100.0) {
        delta_x_delta_y_high.Fill(delta_x, delta_y);
      }
    }
    if (all_count > 0U) {
      ++summary.triggers_with_reconstruction;
      all15_multiplicity.Fill(all_count);
    }
    if (high_count > 0U) {
      ++summary.triggers_with_high_energy_reconstruction;
      all15_high_multiplicity.Fill(high_count);
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
  for (std::vector<TH1*>::const_iterator object = objects.begin();
       object != objects.end(); ++object) {
    (*object)->Write();
  }
  output->Close();
  WriteReport(definition, summary, output_root_file, report_file);
  return summary;
}

}  // namespace cshine_gamma
