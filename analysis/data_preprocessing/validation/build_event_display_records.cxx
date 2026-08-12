#include <TChain.h>
#include <TFile.h>
#include <TH2F.h>
#include <TNamed.h>
#include <TParameter.h>
#include <TSystem.h>
#include <TTree.h>
#include <TTreeReader.h>
#include <TTreeReaderArray.h>
#include <TTreeReaderValue.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "jiugong_recon.h"

namespace {

constexpr unsigned int kCrystalCount = 15U;
constexpr unsigned int kCosmicMinimumFiredCrystals = 5U;
constexpr double kCosmicMinimumRawEnergyMeV = 100.0;
constexpr unsigned int kCosmicAcceptedOrdinal = 2U;
constexpr double kGammaMinimumReconstructedEnergyMeV = 110.0;
constexpr double kGammaMaximumReconstructedEnergyMeV = 200.0;
constexpr unsigned int kGammaAcceptedOrdinal = 16U;
constexpr unsigned short kCentralCores[] = {5, 6, 9, 10};
constexpr unsigned short kSideCores[] = {4, 7, 8, 11, 13, 14};

struct SelectedEvent {
  Long64_t source_entry = -1;
  unsigned int accepted_ordinal = 0U;
  unsigned int fired_crystals = 0U;
  unsigned short veto_count = 0U;
  short reconstructed_center = -1;
  double raw_display_energy_mev = 0.0;
  double reconstructed_energy_mev =
      std::numeric_limits<double>::quiet_NaN();
  std::array<double, kCrystalCount> energy{};
  std::array<double, kCrystalCount> time{};
};

bool contains(const unsigned short* begin, const unsigned short* end,
              short value) {
  return std::find(begin, end, static_cast<unsigned short>(value)) != end;
}

bool is_central(short center) {
  return center >= 0 &&
         contains(std::begin(kCentralCores), std::end(kCentralCores), center);
}

bool is_side(short center) {
  return center >= 0 &&
         contains(std::begin(kSideCores), std::end(kSideCores), center);
}

std::string run_file(const std::string& directory, int date, int run) {
  std::ostringstream path;
  path << directory << "/a" << date << "_SnSn_GOAL_ALLCOIN."
       << std::setfill('0') << std::setw(3) << run << ".root";
  return path.str();
}

std::vector<std::string> gamma_input_files(const std::string& analysis_root) {
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
  if (files.size() != 60U) {
    throw std::runtime_error("Gamma event-display input must contain 60 files.");
  }
  return files;
}

std::string cosmic_input_file(const std::string& analysis_root) {
  return analysis_root +
         "/DataPreprocessing/step4-convert.0308/"
         "a20240306_SnSn_GOAL_ALLCOIN.007.root";
}

void require_file(const std::string& path) {
  if (gSystem->AccessPathName(path.c_str())) {
    throw std::runtime_error("Input file not found: " + path);
  }
}

void ensure_parent_directory(const std::string& path) {
  const std::string::size_type slash = path.find_last_of('/');
  if (slash == std::string::npos) {
    return;
  }
  const std::string parent = path.substr(0, slash);
  if (!parent.empty() && gSystem->mkdir(parent.c_str(), true) != 0 &&
      gSystem->AccessPathName(parent.c_str())) {
    throw std::runtime_error("Cannot create output directory: " + parent);
  }
}

template <class EnergyArray, class TimeArray>
SelectedEvent copy_event(const EnergyArray& energy, const TimeArray& time,
                         Long64_t entry, unsigned int ordinal,
                         unsigned short veto_count) {
  if (energy.GetSize() < kCrystalCount || time.GetSize() < kCrystalCount) {
    throw std::runtime_error(
        "GammaEnergy and GammaTime must each contain 15 values.");
  }
  SelectedEvent selected;
  selected.source_entry = entry;
  selected.accepted_ordinal = ordinal;
  selected.veto_count = veto_count;
  for (unsigned int crystal = 0; crystal < kCrystalCount; ++crystal) {
    selected.energy[crystal] = energy[crystal];
    selected.time[crystal] = time[crystal];
    if (!std::isnan(selected.time[crystal])) {
      ++selected.fired_crystals;
      selected.raw_display_energy_mev += selected.energy[crystal];
    }
  }
  return selected;
}

SelectedEvent select_cosmic_event(const std::string& input_path) {
  require_file(input_path);
  TFile input(input_path.c_str(), "READ");
  if (input.IsZombie()) {
    throw std::runtime_error("Cannot open cosmic event-display input: " +
                             input_path);
  }
  TTree* tree = nullptr;
  input.GetObject("GammaCaliData", tree);
  if (!tree) {
    throw std::runtime_error("Missing GammaCaliData tree: " + input_path);
  }

  TTreeReader reader(tree);
  TTreeReaderArray<Double_t> energy(reader, "GammaEnergy");
  TTreeReaderArray<Double_t> time(reader, "GammaTime");
  TTreeReaderValue<UShort_t> veto_count(reader, "count_veto");
  unsigned int accepted = 0U;
  while (reader.Next()) {
    const SelectedEvent candidate = copy_event(
        energy, time, reader.GetCurrentEntry(), accepted + 1U, *veto_count);
    if (candidate.veto_count != 0U ||
        candidate.raw_display_energy_mev < kCosmicMinimumRawEnergyMeV ||
        candidate.fired_crystals < kCosmicMinimumFiredCrystals) {
      continue;
    }
    ++accepted;
    if (accepted == kCosmicAcceptedOrdinal) {
      SelectedEvent result = candidate;
      result.accepted_ordinal = accepted;
      return result;
    }
  }
  throw std::runtime_error("Cosmic event-display selection found too few events.");
}

SelectedEvent select_gamma_event(const std::vector<std::string>& files) {
  TChain chain("GammaCaliData", "Chained calibrated gamma data");
  for (const std::string& path : files) {
    require_file(path);
    if (chain.Add(path.c_str()) != 1) {
      throw std::runtime_error("Cannot add gamma event-display input: " + path);
    }
  }

  TTreeReader reader(&chain);
  TTreeReaderArray<Double_t> energy(reader, "GammaEnergy");
  TTreeReaderArray<Double_t> time(reader, "GammaTime");
  TTreeReaderValue<UShort_t> veto_count(reader, "count_veto");
  unsigned int accepted = 0U;
  while (reader.Next()) {
    if (energy.GetSize() < kCrystalCount || time.GetSize() < kCrystalCount) {
      throw std::runtime_error(
          "GammaEnergy and GammaTime must each contain 15 values.");
    }
    const auto reconstructed = jiugong_recon(energy, time);
    for (const auto& item : reconstructed) {
      const short center = item.second.GetCenter();
      if (!is_central(center) && !is_side(center)) {
        continue;
      }
      if (is_side(center) && *veto_count > 0U) {
        continue;
      }
      const double reconstructed_energy = item.second.GetEnergy();
      if (reconstructed_energy < kGammaMinimumReconstructedEnergyMeV ||
          reconstructed_energy > kGammaMaximumReconstructedEnergyMeV) {
        continue;
      }

      ++accepted;
      if (accepted == kGammaAcceptedOrdinal) {
        SelectedEvent result = copy_event(
            energy, time, reader.GetCurrentEntry(), accepted, *veto_count);
        result.reconstructed_center = center;
        result.reconstructed_energy_mev = reconstructed_energy;
        return result;
      }
      // The historical h2_check.C records at most one accepted candidate from
      // each tree entry before advancing to the next entry.
      break;
    }
  }
  throw std::runtime_error("Gamma event-display selection found too few events.");
}

TH2F make_display_histogram(const char* name, const SelectedEvent& event) {
  // The historical EventDisplay.C and h2_check.C both store the 4x4 display
  // as TH2F.  Retaining that type preserves the historical float rounding.
  TH2F histogram(name, "", 4, 0.0, 4.0, 4, 0.0, 4.0);
  histogram.SetDirectory(nullptr);
  for (unsigned int crystal = 0; crystal < kCrystalCount; ++crystal) {
    if (std::isnan(event.time[crystal])) {
      continue;
    }
    const int mapped = crystal == 12U ? 0 : 15 - static_cast<int>(crystal);
    const int x_index = mapped % 4;
    const int y_index = mapped / 4;
    histogram.SetBinContent(x_index + 1, 4 - y_index,
                            event.energy[crystal]);
  }
  return histogram;
}

void write_event_metadata(const char* prefix, const SelectedEvent& event) {
  TParameter<Long64_t>((std::string(prefix) + "_source_entry").c_str(),
                       event.source_entry)
      .Write();
  TParameter<int>((std::string(prefix) + "_accepted_ordinal").c_str(),
                  static_cast<int>(event.accepted_ordinal))
      .Write();
  TParameter<int>((std::string(prefix) + "_fired_crystals").c_str(),
                  static_cast<int>(event.fired_crystals))
      .Write();
  TParameter<int>((std::string(prefix) + "_veto_count").c_str(),
                  static_cast<int>(event.veto_count))
      .Write();
  TParameter<int>((std::string(prefix) + "_reconstructed_center").c_str(),
                  static_cast<int>(event.reconstructed_center))
      .Write();
  TParameter<double>((std::string(prefix) + "_raw_display_energy_mev").c_str(),
                     event.raw_display_energy_mev)
      .Write();
  TParameter<double>(
      (std::string(prefix) + "_reconstructed_energy_mev").c_str(),
      event.reconstructed_energy_mev)
      .Write();
}

void write_report(const std::string& report_path,
                  const std::string& cosmic_path,
                  const std::vector<std::string>& gamma_files,
                  const SelectedEvent& gamma,
                  const SelectedEvent& cosmic) {
  ensure_parent_directory(report_path);
  std::ofstream report(report_path.c_str());
  if (!report) {
    throw std::runtime_error("Cannot create run report: " + report_path);
  }
  report << "field\tgamma\tcosmic\n";
  report << "selection\treconstructed-candidate\traw-crystal-event\n";
  report << "accepted_ordinal\t" << gamma.accepted_ordinal << '\t'
         << cosmic.accepted_ordinal << '\n';
  report << "source_entry_zero_based\t" << gamma.source_entry << '\t'
         << cosmic.source_entry << '\n';
  report << "fired_crystals\t" << gamma.fired_crystals << '\t'
         << cosmic.fired_crystals << '\n';
  report << "veto_count\t" << gamma.veto_count << '\t'
         << cosmic.veto_count << '\n';
  report << "reconstructed_center\t" << gamma.reconstructed_center << '\t'
         << cosmic.reconstructed_center << '\n';
  report << std::setprecision(17);
  report << "raw_display_energy_mev\t" << gamma.raw_display_energy_mev << '\t'
         << cosmic.raw_display_energy_mev << '\n';
  report << "reconstructed_energy_mev\t"
         << gamma.reconstructed_energy_mev << '\t'
         << cosmic.reconstructed_energy_mev << '\n';
  report << "cosmic_input\t\t" << cosmic_path << '\n';
  report << "gamma_input_count\t" << gamma_files.size() << "\t\n";
  for (std::size_t index = 0; index < gamma_files.size(); ++index) {
    report << "gamma_input_" << std::setw(2) << std::setfill('0') << index
           << '\t' << gamma_files[index] << "\t\n";
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 4 || argc > 5) {
    std::cerr << "Usage: " << argv[0]
              << " ANALYSIS_ROOT OUTPUT_ROOT RUN_REPORT [--force]" << std::endl;
    return 2;
  }
  try {
    const std::string analysis_root = argv[1];
    const std::string output_path = argv[2];
    const std::string report_path = argv[3];
    const bool force = argc == 5 && std::string(argv[4]) == "--force";
    if (argc == 5 && !force) {
      throw std::invalid_argument("The only supported fourth argument is --force.");
    }
    if (!force && (!gSystem->AccessPathName(output_path.c_str()) ||
                   !gSystem->AccessPathName(report_path.c_str()))) {
      throw std::runtime_error(
          "Output already exists; pass --force only for an intentional replacement.");
    }

    const std::string cosmic_path = cosmic_input_file(analysis_root);
    const std::vector<std::string> gamma_files = gamma_input_files(analysis_root);
    const SelectedEvent cosmic = select_cosmic_event(cosmic_path);
    const SelectedEvent gamma = select_gamma_event(gamma_files);
    TH2F gamma_histogram = make_display_histogram("gamma_event_display", gamma);
    TH2F cosmic_histogram =
        make_display_histogram("cosmic_event_display", cosmic);

    ensure_parent_directory(output_path);
    TFile output(output_path.c_str(), force ? "RECREATE" : "CREATE");
    if (output.IsZombie()) {
      throw std::runtime_error("Cannot create output ROOT file: " + output_path);
    }
    gamma_histogram.Write();
    cosmic_histogram.Write();
    write_event_metadata("gamma", gamma);
    write_event_metadata("cosmic", cosmic);
    TNamed("gamma_selection",
           "60-file central sample; jiugong_recon; central or side core; "
           "side veto; 110 <= reconstructed energy <= 200 MeV; accepted "
           "candidate ordinal 16; at most one accepted candidate per entry")
        .Write();
    TNamed("cosmic_selection",
           "a20240306_SnSn_GOAL_ALLCOIN.007.root; global count_veto == 0; "
           "at least 5 crystals with finite GammaTime; raw display energy "
           ">= 100 MeV; accepted event ordinal 2")
        .Write();
    output.Close();
    write_report(report_path, cosmic_path, gamma_files, gamma, cosmic);

    std::cout << "gamma accepted ordinal=" << gamma.accepted_ordinal
              << " entry=" << gamma.source_entry
              << " center=" << gamma.reconstructed_center
              << " reconstructed_energy_mev="
              << gamma.reconstructed_energy_mev << std::endl;
    std::cout << "cosmic accepted ordinal=" << cosmic.accepted_ordinal
              << " entry=" << cosmic.source_entry
              << " fired_crystals=" << cosmic.fired_crystals
              << " raw_display_energy_mev=" << cosmic.raw_display_energy_mev
              << std::endl;
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "ERROR: " << error.what() << std::endl;
    return 1;
  }
}
