// Provenance: DP-S405 and DP-S406,
// DataPreprocessing/step3-time/timeFigs/time_orig.C and time_cali.C.

#include "time_amplitude_spectra.h"

#include "output_path_support.h"
#include "time_calibration.h"

#include <ROOT/TThreadedObject.hxx>
#include <ROOT/TTreeProcessorMT.hxx>
#include <TBranch.h>
#include <TChain.h>
#include <TFile.h>
#include <TH2I.h>
#include <TLeaf.h>
#include <TObjArray.h>
#include <TSystem.h>
#include <TTreeReader.h>
#include <TTreeReaderValue.h>

#include <cmath>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace cshine_gamma {
namespace {

typedef ROOT::TThreadedObject<TH2I> ThreadedHistogram;

std::string JoinPath(const std::string& directory,
                     const std::string& relative_path) {
  if (directory.empty()) {
    return relative_path;
  }
  if (directory[directory.size() - 1] == '/') {
    return directory + relative_path;
  }
  return directory + "/" + relative_path;
}

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

void RequireIntBranch(TChain& chain, const std::string& branch_name) {
  TBranch* branch = chain.GetBranch(branch_name.c_str());
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

bool IsInclusiveHistoricalTdc(const TimeAmplitudeDefinition& definition,
                              int channel) {
  return channel >= definition.tdc_min_inclusive &&
         channel <= definition.tdc_max_inclusive;
}

std::size_t IndividualIndex(const TimeAmplitudeDefinition& definition,
                            unsigned int crystal,
                            unsigned int t0_channel) {
  return static_cast<std::size_t>(crystal) * definition.t0_count +
         t0_channel;
}

double GammaCoordinateNs(const TimeAmplitudeDefinition& definition,
                         unsigned int crystal,
                         int gamma_tdc,
                         int low_gain_adc) {
  const double scaled_time_ns =
      static_cast<double>(gamma_tdc) * definition.gamma_tdc_unit_ns;
  if (definition.mode == TimeAmplitudeMode::kOriginal) {
    return scaled_time_ns;
  }
  return HistoricalDiagnosticMacroGammaTimeNs(
      crystal, scaled_time_ns, static_cast<unsigned short>(low_gain_adc));
}

void WriteRunReport(const TimeAmplitudeDefinition& definition,
                    const TimeAmplitudeSummary& summary,
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
  report << "config\tname\t" << definition.name << "\t\n";
  report << "config\tmode\t" << TimeAmplitudeModeName(definition.mode)
         << "\t\n";
  report << "config\ttree_name\t" << definition.tree_name << "\t\n";
  report << "config\tthreads\t" << definition.thread_count << "\t\n";
  report << "config\ttdc_inclusive\t" << definition.tdc_min_inclusive
         << "\t" << definition.tdc_max_inclusive << "\n";
  report << "config\ttime_histogram\t" << definition.time_bin_count << "\t"
         << definition.time_min_ns << "," << definition.time_max_ns << "\n";
  report << "config\tadc_histogram\t" << definition.adc_bin_count << "\t"
         << definition.adc_min_channel << "," << definition.adc_max_channel
         << "\n";
  for (std::size_t index = 0; index < summary.input_patterns.size(); ++index) {
    report << "input_pattern\t" << summary.input_patterns[index] << "\t"
           << summary.files_added_per_pattern[index] << "\t\n";
  }
  report << "summary\tinput_files\t" << summary.input_file_count << "\t\n";
  report << "summary\ttree_entries\t" << summary.tree_entries << "\t\n";
  for (unsigned int crystal = 0; crystal < definition.channel_count;
       ++crystal) {
    report << "channel\t" << crystal << "\t"
           << summary.average_t0_histogram_entries[crystal];
    for (unsigned int t0 = 0; t0 < definition.t0_count; ++t0) {
      report << "," << summary.individual_t0_histogram_entries[crystal][t0];
    }
    report << "\n";
  }
}

}  // namespace

TimeAmplitudeDefinition CentralOriginalTimeAmplitudeDefinition() {
  TimeAmplitudeDefinition definition;
  definition.name = "central_0303_0310_time_amplitude_original";
  definition.tree_name = "tree";
  definition.input_patterns = {
      "a20240303_SnSn_ALLCOIN.*.root",
      "a20240303_SnSn_GOAL_ALLCOIN.*.root",
      "a20240304_SnSn_GOAL_ALLCOIN.*.root",
      "a20240305_SnSn_GOAL_ALLCOIN.*.root",
      "a20240306_SnSn_GOAL_ALLCOIN.*.root",
      "a20240307_SnSn_GOAL_ALLCOIN.*.root",
      "a20240308_SnSn_GOAL_ALLCOIN.*.root",
      "a20240309_SnSn_GOAL_ALLCOIN.*.root",
      "a20240310_SnSn_GOAL_ALLCOIN.*.root",
  };
  definition.mode = TimeAmplitudeMode::kOriginal;
  definition.channel_count = 15;
  definition.t0_count = 4;
  definition.thread_count = 12;
  definition.time_bin_count = 1000;
  definition.time_min_ns = 0.0;
  definition.time_max_ns = 409.6 + (8.9 / 30.0) * 4096.0;
  definition.adc_bin_count = 2048;
  definition.adc_min_channel = 0.0;
  definition.adc_max_channel = 4096.0;
  definition.tdc_min_inclusive = 100;
  definition.tdc_max_inclusive = 4000;
  definition.gamma_tdc_unit_ns = 8.9 / 30.0;
  definition.t0_tdc_unit_ns = 409.6 / 4096.0;
  definition.t0_offsets_ns = {{6.345, 2.8499, 7.852, 0.0}};
  return definition;
}

TimeAmplitudeDefinition CentralHistoricalCorrectedTimeAmplitudeDefinition() {
  TimeAmplitudeDefinition definition =
      CentralOriginalTimeAmplitudeDefinition();
  definition.name = "central_0303_0310_time_amplitude_historical_corrected";
  definition.mode = TimeAmplitudeMode::kHistoricalDiagnosticCorrected;
  definition.time_bin_count = 500;
  definition.adc_bin_count = 512;
  return definition;
}

const char* TimeAmplitudeModeName(TimeAmplitudeMode mode) {
  if (mode == TimeAmplitudeMode::kOriginal) {
    return "original";
  }
  return "historical-diagnostic-corrected";
}

void ValidateTimeAmplitudeDefinition(
    const TimeAmplitudeDefinition& definition) {
  if (definition.name.empty() || definition.tree_name.empty() ||
      definition.input_patterns.empty()) {
    throw std::invalid_argument("name, tree name, and input patterns are required");
  }
  if (definition.channel_count == 0 || definition.channel_count > 15 ||
      definition.t0_count == 0 || definition.t0_count > 4 ||
      definition.thread_count == 0) {
    throw std::invalid_argument("invalid channel, T0, or thread count");
  }
  if (definition.time_bin_count <= 0 || definition.adc_bin_count <= 0 ||
      !(definition.time_min_ns < definition.time_max_ns) ||
      !(definition.adc_min_channel < definition.adc_max_channel)) {
    throw std::invalid_argument("invalid histogram definition");
  }
  if (definition.tdc_min_inclusive > definition.tdc_max_inclusive ||
      !(definition.gamma_tdc_unit_ns > 0.0) ||
      !(definition.t0_tdc_unit_ns > 0.0)) {
    throw std::invalid_argument("invalid TDC definition");
  }
}

std::string LowGainEnergyBranchName(unsigned int channel) {
  std::ostringstream name;
  name << "GAMMA" << channel + 1 << "_LOW_E";
  return name.str();
}

std::string GammaTimeBranchName(unsigned int channel) {
  std::ostringstream name;
  name << "GAMMA" << channel + 1 << "_T";
  return name.str();
}

std::string T0TimeBranchName(unsigned int channel) {
  std::ostringstream name;
  name << "T0" << channel + 1 << "_T";
  return name.str();
}

std::string IndividualT0HistogramName(unsigned int crystal,
                                      unsigned int t0_channel) {
  std::ostringstream name;
  name << "h_TOF_one_" << crystal << "_c" << t0_channel;
  return name.str();
}

std::string AverageT0HistogramName(TimeAmplitudeMode mode,
                                   unsigned int crystal) {
  std::ostringstream name;
  if (mode == TimeAmplitudeMode::kOriginal) {
    name << "h_TOF_move_";
  } else {
    name << "h_TOF_move_cali_";
  }
  name << crystal;
  return name.str();
}

TimeAmplitudeSummary BuildTimeAmplitudeSpectra(
    const TimeAmplitudeDefinition& definition,
    const std::string& input_directory,
    const std::string& output_root_file,
    const std::string& report_file,
    bool overwrite) {
  ValidateTimeAmplitudeDefinition(definition);
  if (input_directory.empty()) {
    throw std::invalid_argument("input directory must not be empty");
  }
  if (!report_file.empty() && report_file == output_root_file) {
    throw std::invalid_argument("ROOT output and report must use different paths");
  }
  RequireNewOutputPath(output_root_file, overwrite);
  if (!report_file.empty()) {
    RequireNewOutputPath(report_file, overwrite);
  }

  TChain chain(definition.tree_name.c_str(), "converted data");
  TimeAmplitudeSummary summary;
  summary.input_patterns = definition.input_patterns;
  summary.input_file_count = 0;
  for (std::vector<std::string>::const_iterator pattern =
           definition.input_patterns.begin();
       pattern != definition.input_patterns.end(); ++pattern) {
    const int before = chain.GetListOfFiles()->GetEntries();
    chain.Add(JoinPath(input_directory, *pattern).c_str());
    const int after = chain.GetListOfFiles()->GetEntries();
    const int added = after - before;
    if (added <= 0) {
      throw std::runtime_error("input pattern matched no ROOT files: " + *pattern);
    }
    summary.files_added_per_pattern.push_back(added);
    summary.input_file_count += added;
  }
  if (chain.GetEntries() <= 0 || chain.LoadTree(0) < 0) {
    throw std::runtime_error("input chain contains no readable tree entries");
  }
  for (unsigned int crystal = 0; crystal < definition.channel_count;
       ++crystal) {
    RequireIntBranch(chain, LowGainEnergyBranchName(crystal));
    RequireIntBranch(chain, GammaTimeBranchName(crystal));
  }
  for (unsigned int t0 = 0; t0 < definition.t0_count; ++t0) {
    RequireIntBranch(chain, T0TimeBranchName(t0));
  }

  detail::EnsureOutputParentDirectories(
      std::vector<std::string>{output_root_file, report_file});

  std::vector<std::unique_ptr<ThreadedHistogram> > individual_histograms;
  individual_histograms.reserve(
      static_cast<std::size_t>(definition.channel_count) * definition.t0_count);
  std::vector<std::unique_ptr<ThreadedHistogram> > average_histograms;
  average_histograms.reserve(definition.channel_count);
  for (unsigned int crystal = 0; crystal < definition.channel_count;
       ++crystal) {
    for (unsigned int t0 = 0; t0 < definition.t0_count; ++t0) {
      individual_histograms.push_back(std::unique_ptr<ThreadedHistogram>(
          new ThreadedHistogram(
              IndividualT0HistogramName(crystal, t0).c_str(), "",
              definition.time_bin_count, definition.time_min_ns,
              definition.time_max_ns, definition.adc_bin_count,
              definition.adc_min_channel, definition.adc_max_channel)));
    }
    average_histograms.push_back(std::unique_ptr<ThreadedHistogram>(
        new ThreadedHistogram(
            AverageT0HistogramName(definition.mode, crystal).c_str(), "",
            definition.time_bin_count, definition.time_min_ns,
            definition.time_max_ns, definition.adc_bin_count,
            definition.adc_min_channel, definition.adc_max_channel)));
  }

  ROOT::TTreeProcessorMT processor(chain, definition.thread_count);
  auto worker = [&](TTreeReader& reader) {
    std::vector<std::shared_ptr<TH2I> > individual;
    individual.reserve(individual_histograms.size());
    for (std::size_t index = 0; index < individual_histograms.size(); ++index) {
      individual.push_back(individual_histograms[index]->Get());
    }
    std::vector<std::shared_ptr<TH2I> > average;
    average.reserve(average_histograms.size());
    for (std::size_t index = 0; index < average_histograms.size(); ++index) {
      average.push_back(average_histograms[index]->Get());
    }

    std::vector<std::unique_ptr<TTreeReaderValue<Int_t> > > low_gain_adc;
    std::vector<std::unique_ptr<TTreeReaderValue<Int_t> > > gamma_tdc;
    std::vector<std::unique_ptr<TTreeReaderValue<Int_t> > > t0_tdc;
    for (unsigned int crystal = 0; crystal < definition.channel_count;
         ++crystal) {
      low_gain_adc.push_back(std::unique_ptr<TTreeReaderValue<Int_t> >(
          new TTreeReaderValue<Int_t>(reader,
                                      LowGainEnergyBranchName(crystal).c_str())));
      gamma_tdc.push_back(std::unique_ptr<TTreeReaderValue<Int_t> >(
          new TTreeReaderValue<Int_t>(reader,
                                      GammaTimeBranchName(crystal).c_str())));
    }
    for (unsigned int t0 = 0; t0 < definition.t0_count; ++t0) {
      t0_tdc.push_back(std::unique_ptr<TTreeReaderValue<Int_t> >(
          new TTreeReaderValue<Int_t>(reader, T0TimeBranchName(t0).c_str())));
    }

    while (reader.Next()) {
      std::vector<unsigned int> valid_t0_indices;
      std::vector<double> valid_t0_times;
      double t0_sum = 0.0;
      for (unsigned int t0 = 0; t0 < definition.t0_count; ++t0) {
        const int channel = **t0_tdc[t0];
        if (!IsInclusiveHistoricalTdc(definition, channel)) {
          continue;
        }
        const double time_ns =
            static_cast<double>(channel) * definition.t0_tdc_unit_ns +
            definition.t0_offsets_ns[t0];
        valid_t0_indices.push_back(t0);
        valid_t0_times.push_back(time_ns);
        t0_sum += time_ns;
      }
      if (valid_t0_indices.empty()) {
        continue;
      }
      const double average_t0 =
          t0_sum / static_cast<double>(valid_t0_indices.size());

      for (unsigned int crystal = 0; crystal < definition.channel_count;
           ++crystal) {
        const int gamma_time_channel = **gamma_tdc[crystal];
        if (!IsInclusiveHistoricalTdc(definition, gamma_time_channel)) {
          continue;
        }
        const int adc_channel = **low_gain_adc[crystal];
        const double gamma_coordinate = GammaCoordinateNs(
            definition, crystal, gamma_time_channel, adc_channel);
        for (std::size_t index = 0; index < valid_t0_indices.size(); ++index) {
          individual[IndividualIndex(definition, crystal,
                                     valid_t0_indices[index])]
              ->Fill(gamma_coordinate + valid_t0_times[index], adc_channel);
        }
        average[crystal]->Fill(gamma_coordinate + average_t0, adc_channel);
      }
    }
  };
  processor.Process(worker);

  std::unique_ptr<TFile> output(TFile::Open(output_root_file.c_str(), "RECREATE"));
  if (!output || output->IsZombie()) {
    throw std::runtime_error("cannot create ROOT output: " + output_root_file);
  }
  summary.tree_entries = chain.GetEntries();
  summary.average_t0_histogram_entries.resize(definition.channel_count, 0.0);
  summary.individual_t0_histogram_entries.resize(
      definition.channel_count,
      std::vector<double>(definition.t0_count, 0.0));
  for (unsigned int crystal = 0; crystal < definition.channel_count;
       ++crystal) {
    for (unsigned int t0 = 0; t0 < definition.t0_count; ++t0) {
      const std::size_t index = IndividualIndex(definition, crystal, t0);
      std::shared_ptr<TH2I> histogram = individual_histograms[index]->Merge();
      summary.individual_t0_histogram_entries[crystal][t0] =
          histogram->GetEntries();
      output->WriteTObject(histogram.get());
    }
    std::shared_ptr<TH2I> histogram = average_histograms[crystal]->Merge();
    summary.average_t0_histogram_entries[crystal] = histogram->GetEntries();
    output->WriteTObject(histogram.get());
  }
  output->Close();
  WriteRunReport(definition, summary, report_file);
  return summary;
}

}  // namespace cshine_gamma
