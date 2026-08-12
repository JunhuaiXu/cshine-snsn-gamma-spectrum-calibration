#include <ROOT/TThreadedObject.hxx>
#include <ROOT/TTreeProcessorMT.hxx>
#include <TChain.h>
#include <TFile.h>
#include <TH1I.h>
#include <TTreeReader.h>
#include <TTreeReaderArray.h>
#include <TTreeReaderValue.h>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "jiugong_recon.h"

namespace {

constexpr double kHighEnergyThresholdMeV = 35.0;
constexpr unsigned short kCentralCores[] = {5, 6, 9, 10};
constexpr unsigned short kSideCores[] = {4, 7, 8, 11, 13, 14};

bool contains(const unsigned short* begin,
              const unsigned short* end,
              unsigned short value) {
  return std::find(begin, end, value) != end;
}

bool is_accepted_core(unsigned short core) {
  if (contains(std::begin(kCentralCores), std::end(kCentralCores), core)) {
    return true;
  }
  if (contains(std::begin(kSideCores), std::end(kSideCores), core)) {
    return true;
  }
  return false;
}

std::string run_file(const std::string& input_directory, int date, int run) {
  std::ostringstream path;
  path << input_directory << "/a" << date
       << "_SnSn_GOAL_ALLCOIN." << std::setfill('0') << std::setw(3) << run
       << ".root";
  return path.str();
}

std::vector<std::string> input_files(const std::string& analysis_root) {
  const std::string directory =
      analysis_root + "/DataPreprocessing/step4-convert.0308.PreRun";
  std::vector<std::string> files;
  files.push_back(run_file(directory, 20240304, 6));
  for (int run = 0; run <= 7; ++run) {
    files.push_back(run_file(directory, 20240305, run));
  }
  for (int run = 0; run <= 14; ++run) {
    files.push_back(run_file(directory, 20240306, run));
  }
  for (int run = 0; run <= 13; ++run) {
    files.push_back(run_file(directory, 20240307, run));
  }
  for (int run = 0; run <= 10; ++run) {
    files.push_back(run_file(directory, 20240308, run));
  }
  for (int run = 0; run <= 3; ++run) {
    files.push_back(run_file(directory, 20240309, run));
  }
  for (int run = 0; run <= 6; ++run) {
    files.push_back(run_file(directory, 20240310, run));
  }
  return files;
}

void write_counts(const char* label, const TH1I& histogram) {
  std::cout << label;
  for (int bin = 1; bin <= histogram.GetNbinsX(); ++bin) {
    std::cout << (bin == 1 ? " " : ",")
              << static_cast<long long>(histogram.GetBinContent(bin));
  }
  std::cout << std::endl;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3 || argc > 4) {
    std::cerr << "Usage: " << argv[0]
              << " ANALYSIS_ROOT OUTPUT_ROOT [THREADS]" << std::endl;
    return 2;
  }

  const std::string analysis_root = argv[1];
  const std::string output_path = argv[2];
  const unsigned int threads =
      argc == 4 ? static_cast<unsigned int>(std::stoul(argv[3])) : 22U;
  if (threads == 0U) {
    throw std::invalid_argument("THREADS must be a positive integer.");
  }

  TChain chain("GammaCaliData", "Chained calibrated gamma data");
  const std::vector<std::string> files = input_files(analysis_root);
  for (const std::string& path : files) {
    if (chain.Add(path.c_str()) != 1) {
      throw std::runtime_error("Cannot add input ROOT file: " + path);
    }
  }
  if (files.size() != 60U) {
    throw std::runtime_error("The validation input list must contain 60 files.");
  }

  ROOT::TThreadedObject<TH1I> all_core_multiplicity(
      "all_core_multiplicity", "", 9, 0.5, 9.5);
  ROOT::TThreadedObject<TH1I> high_energy_core_multiplicity(
      "high_energy_core_multiplicity", "", 9, 0.5, 9.5);
  ROOT::TThreadedObject<TH1I> all_cores_high_multiplicity(
      "all_cores_high_multiplicity", "", 9, 0.5, 9.5);

  ROOT::TTreeProcessorMT processor(chain, threads);
  processor.Process([&](TTreeReader& reader) {
    TTreeReaderArray<Double_t> gamma_energy(reader, "GammaEnergy");
    TTreeReaderArray<Double_t> gamma_time(reader, "GammaTime");
    auto all_histogram = all_core_multiplicity.Get();
    auto high_histogram = high_energy_core_multiplicity.Get();
    auto old_histogram = all_cores_high_multiplicity.Get();

    while (reader.Next()) {
      if (gamma_energy.GetSize() < 15U || gamma_time.GetSize() < 15U) {
        throw std::runtime_error(
            "GammaEnergy and GammaTime must each contain 15 crystal values.");
      }
      const auto reconstructed = jiugong_recon(gamma_energy, gamma_time);
      unsigned int all_count = 0;
      unsigned int high_count = 0;

      for (const auto& candidate : reconstructed) {
        const int center = candidate.second.GetCenter();
        if (center < 0) {
          continue;
        }
        const unsigned short valid_center = static_cast<unsigned short>(center);
        if (!is_accepted_core(valid_center)) {
          continue;
        }
        ++all_count;
        if (candidate.second.GetEnergy() > kHighEnergyThresholdMeV) {
          ++high_count;
        }
      }

      if (all_count > 0U) {
        all_histogram->Fill(all_count);
      }
      if (high_count > 0U) {
        high_histogram->Fill(high_count);
      }

      // Retain the historical implementation as a diagnostic comparator: it
      // counted a trigger only when every accepted core exceeded 35 MeV.
      if (all_count > 0U && high_count == all_count) {
        old_histogram->Fill(all_count);
      }
    }
  });

  const std::shared_ptr<TH1I> all_merged = all_core_multiplicity.Merge();
  const std::shared_ptr<TH1I> high_merged = high_energy_core_multiplicity.Merge();
  const std::shared_ptr<TH1I> old_merged = all_cores_high_multiplicity.Merge();

  TFile output(output_path.c_str(), "CREATE");
  if (output.IsZombie()) {
    throw std::runtime_error("Cannot create output ROOT file: " + output_path);
  }
  all_merged->Write();
  high_merged->Write();
  old_merged->Write();
  output.Close();

  std::cout << "input_files " << files.size() << std::endl;
  std::cout << "tree_entries " << chain.GetEntries() << std::endl;
  std::cout << "threshold_MeV " << kHighEnergyThresholdMeV << std::endl;
  write_counts("all", *all_merged);
  write_counts("high_correct", *high_merged);
  write_counts("all_cores_high_old", *old_merged);
  return 0;
}
