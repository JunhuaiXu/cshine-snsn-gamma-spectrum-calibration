#include <ROOT/TThreadedObject.hxx>
#include <ROOT/TTreeProcessorMT.hxx>
#include <TChain.h>
#include <TFile.h>
#include <TH1D.h>
#include <TH2D.h>
#include <TTreeReader.h>
#include <TTreeReaderArray.h>
#include <TTreeReaderValue.h>

#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "jiugong_recon.h"

namespace {

constexpr double kHighEnergyThresholdMeV = 35.0;
constexpr unsigned int kCrystalCount = 15U;
constexpr unsigned int kMaskCount = 1U << kCrystalCount;
constexpr unsigned short kOfficialCores[] = {4, 5, 6, 7, 8,
                                              9, 10, 11, 13, 14};

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

unsigned int official_mask() {
  unsigned int mask = 0U;
  for (const unsigned short core : kOfficialCores) {
    mask |= 1U << core;
  }
  return mask;
}

long long mask_frequency(const TH1D& histogram, unsigned int mask) {
  return static_cast<long long>(
      std::llround(histogram.GetBinContent(static_cast<int>(mask) + 1)));
}

using MaskFrequency = std::pair<unsigned int, long long>;

std::vector<MaskFrequency> nonzero_masks(const TH1D& histogram) {
  std::vector<MaskFrequency> result;
  for (unsigned int mask = 0U; mask < kMaskCount; ++mask) {
    const long long frequency = mask_frequency(histogram, mask);
    if (frequency > 0) {
      result.emplace_back(mask, frequency);
    }
  }
  return result;
}

std::pair<long long, long long> multiplicity_one_two(
    const std::vector<MaskFrequency>& masks,
    unsigned int selection_mask) {
  long long one = 0;
  long long two = 0;
  for (const auto& item : masks) {
    const int multiplicity = __builtin_popcount(item.first & selection_mask);
    if (multiplicity == 1) {
      one += item.second;
    } else if (multiplicity == 2) {
      two += item.second;
    }
  }
  return std::make_pair(one, two);
}

std::string selected_cores(unsigned int mask) {
  std::ostringstream result;
  bool first = true;
  for (unsigned int core = 0U; core < kCrystalCount; ++core) {
    if ((mask & (1U << core)) == 0U) {
      continue;
    }
    if (!first) {
      result << ',';
    }
    result << core;
    first = false;
  }
  return result.str();
}

void print_core_breakdown(const std::vector<MaskFrequency>& masks,
                          unsigned int scope_mask) {
  long long single_owner[kCrystalCount] = {};
  long long double_participation[kCrystalCount] = {};
  for (const auto& item : masks) {
    const unsigned int selected = item.first & scope_mask;
    const int multiplicity = __builtin_popcount(selected);
    if (multiplicity != 1 && multiplicity != 2) {
      continue;
    }
    for (unsigned int core = 0U; core < kCrystalCount; ++core) {
      if ((selected & (1U << core)) == 0U) {
        continue;
      }
      if (multiplicity == 1) {
        single_owner[core] += item.second;
      } else {
        double_participation[core] += item.second;
      }
    }
  }

  std::cout << "core,single_core_triggers,double_core_participation" << std::endl;
  for (unsigned int core = 0U; core < kCrystalCount; ++core) {
    if ((scope_mask & (1U << core)) == 0U) {
      continue;
    }
    std::cout << core << ',' << single_owner[core] << ','
              << double_participation[core] << std::endl;
  }
}

unsigned int print_exact_matches(const std::vector<MaskFrequency>& masks,
                                 const char* label) {
  unsigned int match_count = 0U;
  for (unsigned int selection = 1U; selection < kMaskCount; ++selection) {
    const auto counts = multiplicity_one_two(masks, selection);
    if (counts.first == 6731 && counts.second == 19) {
      std::cout << label << ' ' << selected_cores(selection) << std::endl;
      ++match_count;
    }
  }
  return match_count;
}

void print_histogram_counts(const char* label, const TH1D& histogram) {
  std::cout << label;
  for (int bin = 1; bin <= histogram.GetNbinsX(); ++bin) {
    std::cout << (bin == 1 ? " " : ",")
              << static_cast<long long>(
                     std::llround(histogram.GetBinContent(bin)));
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
    throw std::runtime_error("The diagnostic input list must contain 60 files.");
  }

  ROOT::TThreadedObject<TH1D> high_core_masks_strict(
      "high_core_mask_frequency", "", static_cast<int>(kMaskCount), -0.5,
      static_cast<double>(kMaskCount) - 0.5);
  ROOT::TThreadedObject<TH1D> high_core_masks_inclusive(
      "high_core_mask_frequency_ge", "", static_cast<int>(kMaskCount), -0.5,
      static_cast<double>(kMaskCount) - 0.5);
  ROOT::TThreadedObject<TH1D> high_core_masks_veto(
      "high_core_mask_frequency_veto", "", static_cast<int>(kMaskCount), -0.5,
      static_cast<double>(kMaskCount) - 0.5);
  ROOT::TThreadedObject<TH2D> all15_cluster_size_vs_total_energy(
      "all15_cluster_size_vs_total_energy", "", 200, 0.0, 200.0, 9, 0.5,
      9.5);
  ROOT::TThreadedObject<TH1D> all15_core_multiplicity(
      "all15_core_multiplicity", "", 9, 0.5, 9.5);
  ROOT::TThreadedObject<TH1D> all15_high_core_multiplicity(
      "all15_high_core_multiplicity", "", 9, 0.5, 9.5);

  ROOT::TTreeProcessorMT processor(chain, threads);
  processor.Process([&](TTreeReader& reader) {
    TTreeReaderArray<Double_t> gamma_energy(reader, "GammaEnergy");
    TTreeReaderArray<Double_t> gamma_time(reader, "GammaTime");
    TTreeReaderValue<UShort_t> veto_count(reader, "count_veto");
    auto strict_histogram = high_core_masks_strict.Get();
    auto inclusive_histogram = high_core_masks_inclusive.Get();
    auto veto_histogram = high_core_masks_veto.Get();
    auto cluster_histogram = all15_cluster_size_vs_total_energy.Get();
    auto all_multiplicity_histogram = all15_core_multiplicity.Get();
    auto high_multiplicity_histogram = all15_high_core_multiplicity.Get();

    while (reader.Next()) {
      if (gamma_energy.GetSize() < kCrystalCount ||
          gamma_time.GetSize() < kCrystalCount) {
        throw std::runtime_error(
            "GammaEnergy and GammaTime must each contain 15 crystal values.");
      }
      const auto reconstructed = jiugong_recon(gamma_energy, gamma_time);
      unsigned int strict_mask = 0U;
      unsigned int inclusive_mask = 0U;
      unsigned int all_count = 0U;
      unsigned int high_count = 0U;
      for (const auto& candidate : reconstructed) {
        const int center = candidate.second.GetCenter();
        if (center < 0 || center >= static_cast<int>(kCrystalCount)) {
          continue;
        }
        ++all_count;
        cluster_histogram->Fill(candidate.second.GetEnergy(),
                                candidate.second.GetMultiplicity());
        if (candidate.second.GetEnergy() > kHighEnergyThresholdMeV) {
          strict_mask |= 1U << static_cast<unsigned int>(center);
          ++high_count;
        }
        if (candidate.second.GetEnergy() >= kHighEnergyThresholdMeV) {
          inclusive_mask |= 1U << static_cast<unsigned int>(center);
        }
      }
      strict_histogram->Fill(static_cast<double>(strict_mask));
      inclusive_histogram->Fill(static_cast<double>(inclusive_mask));
      veto_histogram->Fill(
          static_cast<double>(*veto_count == 0U ? strict_mask : 0U));
      if (all_count > 0U) {
        all_multiplicity_histogram->Fill(all_count);
      }
      if (high_count > 0U) {
        high_multiplicity_histogram->Fill(high_count);
      }
    }
  });

  const std::shared_ptr<TH1D> strict_merged = high_core_masks_strict.Merge();
  const std::shared_ptr<TH1D> inclusive_merged = high_core_masks_inclusive.Merge();
  const std::shared_ptr<TH1D> veto_merged = high_core_masks_veto.Merge();
  const std::shared_ptr<TH2D> cluster_merged =
      all15_cluster_size_vs_total_energy.Merge();
  const std::shared_ptr<TH1D> all_multiplicity_merged =
      all15_core_multiplicity.Merge();
  const std::shared_ptr<TH1D> high_multiplicity_merged =
      all15_high_core_multiplicity.Merge();
  TFile output(output_path.c_str(), "CREATE");
  if (output.IsZombie()) {
    throw std::runtime_error("Cannot create output ROOT file: " + output_path);
  }
  strict_merged->Write();
  inclusive_merged->Write();
  veto_merged->Write();
  cluster_merged->Write();
  all_multiplicity_merged->Write();
  high_multiplicity_merged->Write();
  output.Close();

  const unsigned int ten_core_mask = official_mask();
  const unsigned int all_core_mask = kMaskCount - 1U;
  const std::vector<MaskFrequency> strict_masks = nonzero_masks(*strict_merged);
  const std::vector<MaskFrequency> inclusive_masks =
      nonzero_masks(*inclusive_merged);
  const std::vector<MaskFrequency> veto_masks = nonzero_masks(*veto_merged);
  const auto ten_core_counts = multiplicity_one_two(strict_masks, ten_core_mask);
  const auto all_core_counts = multiplicity_one_two(strict_masks, all_core_mask);
  const auto ten_core_counts_ge =
      multiplicity_one_two(inclusive_masks, ten_core_mask);
  const auto all_core_counts_ge =
      multiplicity_one_two(inclusive_masks, all_core_mask);
  const auto ten_core_counts_veto =
      multiplicity_one_two(veto_masks, ten_core_mask);
  const auto all_core_counts_veto =
      multiplicity_one_two(veto_masks, all_core_mask);

  std::cout << "input_files " << files.size() << std::endl;
  std::cout << "tree_entries " << chain.GetEntries() << std::endl;
  std::cout << "threshold_MeV " << kHighEnergyThresholdMeV << std::endl;
  std::cout << "ten_core_mask " << selected_cores(ten_core_mask) << std::endl;
  std::cout << "ten_core_N1_N2 " << ten_core_counts.first << ','
            << ten_core_counts.second << std::endl;
  std::cout << "all15_N1_N2 " << all_core_counts.first << ','
            << all_core_counts.second << std::endl;
  std::cout << "ten_core_N1_N2_ge " << ten_core_counts_ge.first << ','
            << ten_core_counts_ge.second << std::endl;
  std::cout << "all15_N1_N2_ge " << all_core_counts_ge.first << ','
            << all_core_counts_ge.second << std::endl;
  std::cout << "ten_core_N1_N2_veto " << ten_core_counts_veto.first << ','
            << ten_core_counts_veto.second << std::endl;
  std::cout << "all15_N1_N2_veto " << all_core_counts_veto.first << ','
            << all_core_counts_veto.second << std::endl;
  print_histogram_counts("all15_all_multiplicity", *all_multiplicity_merged);
  print_histogram_counts("all15_high_multiplicity", *high_multiplicity_merged);

  std::cout << "ten_core_breakdown" << std::endl;
  print_core_breakdown(strict_masks, ten_core_mask);
  std::cout << "all15_breakdown" << std::endl;
  print_core_breakdown(strict_masks, all_core_mask);
  std::cout << "all15_veto_breakdown" << std::endl;
  print_core_breakdown(veto_masks, all_core_mask);

  const unsigned int strict_matches =
      print_exact_matches(strict_masks, "exact_6731_19_subset");
  const unsigned int inclusive_matches =
      print_exact_matches(inclusive_masks, "exact_6731_19_subset_ge");
  const unsigned int veto_matches =
      print_exact_matches(veto_masks, "exact_6731_19_subset_veto");
  std::cout << "exact_6731_19_subset_count " << strict_matches << std::endl;
  std::cout << "exact_6731_19_subset_count_ge " << inclusive_matches
            << std::endl;
  std::cout << "exact_6731_19_subset_count_veto " << veto_matches
            << std::endl;
  return 0;
}
