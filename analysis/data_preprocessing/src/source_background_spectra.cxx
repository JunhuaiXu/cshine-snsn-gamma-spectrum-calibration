// Provenance: DP-S000,
// DataPreprocessing/step0-nobkg/20240308-ThnatCo60-ALLOR_NoBkg.C.

#include "source_background_spectra.h"

#include "output_path_support.h"

#include <TChain.h>
#include <TFile.h>
#include <TH1D.h>
#include <TH1I.h>
#include <TSystem.h>
#include <TTree.h>

#include <cmath>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace cshine_gamma {
namespace {

std::string FormatChannelObjectName(const char* prefix, unsigned int channel) {
  std::ostringstream name;
  name << prefix << std::setfill('0') << std::setw(2) << channel;
  return name.str();
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

void RequireReadableFiles(const std::vector<std::string>& files,
                          const char* sample_label) {
  for (std::vector<std::string>::const_iterator file = files.begin();
       file != files.end(); ++file) {
    if (gSystem->AccessPathName(file->c_str(), kReadPermission)) {
      throw std::runtime_error(std::string("missing or unreadable ") +
                               sample_label + " file: " + *file);
    }
  }
}

void RequireInputSchemas(const std::vector<std::string>& files,
                         const SourceBackgroundDefinition& definition,
                         const char* sample_label) {
  for (std::vector<std::string>::const_iterator file = files.begin();
       file != files.end(); ++file) {
    std::unique_ptr<TFile> input(TFile::Open(file->c_str(), "READ"));
    if (!input || input->IsZombie()) {
      throw std::runtime_error(std::string("cannot open ") + sample_label +
                               " file: " + *file);
    }
    TTree* tree = nullptr;
    input->GetObject(definition.tree_name.c_str(), tree);
    if (tree == nullptr) {
      throw std::runtime_error(std::string("missing tree '") +
                               definition.tree_name + "' in " + *file);
    }
    for (unsigned int channel = 0; channel < definition.channel_count;
         ++channel) {
      const std::string energy_branch = HighGainEnergyBranchName(channel);
      const std::string time_branch = TimeBranchName(channel);
      if (tree->GetBranch(energy_branch.c_str()) == nullptr) {
        throw std::runtime_error("missing branch " + energy_branch + " in " +
                                 *file);
      }
      if (tree->GetBranch(time_branch.c_str()) == nullptr) {
        throw std::runtime_error("missing branch " + time_branch + " in " +
                                 *file);
      }
    }
  }
}

void AddFiles(TChain& chain,
              const std::vector<std::string>& files,
              const char* sample_label) {
  for (std::vector<std::string>::const_iterator file = files.begin();
       file != files.end(); ++file) {
    if (chain.AddFile(file->c_str()) != 1) {
      throw std::runtime_error(std::string("failed to add ") + sample_label +
                               " file to TChain: " + *file);
    }
  }
}

void RequireBranches(TChain& chain,
                     const SourceBackgroundDefinition& definition,
                     const char* sample_label) {
  if (chain.GetEntries() < 0 || chain.LoadTree(0) < 0) {
    throw std::runtime_error(std::string("cannot load ") + sample_label +
                             " tree '" + definition.tree_name + "'");
  }

  for (unsigned int channel = 0; channel < definition.channel_count;
       ++channel) {
    const std::string energy_branch = HighGainEnergyBranchName(channel);
    const std::string time_branch = TimeBranchName(channel);
    if (chain.GetBranch(energy_branch.c_str()) == nullptr) {
      throw std::runtime_error(std::string("missing ") + sample_label +
                               " branch: " + energy_branch);
    }
    if (chain.GetBranch(time_branch.c_str()) == nullptr) {
      throw std::runtime_error(std::string("missing ") + sample_label +
                               " branch: " + time_branch);
    }
  }
}

double AllBins(const TH1& histogram) {
  return histogram.Integral(0, histogram.GetNbinsX() + 1);
}

}  // namespace

SourceBackgroundDefinition Central0308SourceBackgroundDefinition() {
  SourceBackgroundDefinition definition;
  definition.name = "central_0308";
  definition.tree_name = "tree";
  definition.source.prefix = "20240308_ThnatCo60";
  definition.source.first_file = 0;
  definition.source.last_file = 20;
  definition.source.live_time_seconds = 3.0 * (58166.0 - 57277.0);
  definition.background.prefix = "20240308_BKG_ALLOR";
  definition.background.first_file = 0;
  definition.background.last_file = 23;
  definition.background.live_time_seconds = 3.0 * (57273.0 - 55851.0);
  definition.channel_count = 15;
  definition.adc_bin_count = 4096;
  definition.adc_min = 0.0;
  definition.adc_max = 4096.0;
  definition.time_min_exclusive = 100.0;
  definition.time_max_exclusive = 4000.0;
  return definition;
}

void ValidateSourceBackgroundDefinition(
    const SourceBackgroundDefinition& definition) {
  if (definition.name.empty() || definition.tree_name.empty()) {
    throw std::invalid_argument("definition name and tree name are required");
  }
  if (definition.source.prefix.empty() ||
      definition.background.prefix.empty()) {
    throw std::invalid_argument("source and background prefixes are required");
  }
  if (definition.source.first_file > definition.source.last_file ||
      definition.background.first_file > definition.background.last_file) {
    throw std::invalid_argument("invalid input file range");
  }
  if (!(definition.source.live_time_seconds > 0.0) ||
      !(definition.background.live_time_seconds > 0.0) ||
      !std::isfinite(definition.source.live_time_seconds) ||
      !std::isfinite(definition.background.live_time_seconds)) {
    throw std::invalid_argument("live times must be finite and positive");
  }
  if (definition.channel_count == 0 || definition.adc_bin_count <= 0 ||
      !(definition.adc_min < definition.adc_max) ||
      !(definition.time_min_exclusive < definition.time_max_exclusive)) {
    throw std::invalid_argument("invalid channel, histogram, or time range");
  }
}

std::string FormatInputFilename(const std::string& input_directory,
                                const std::string& sample_prefix,
                                int file_index) {
  std::ostringstream path;
  path << input_directory;
  if (!input_directory.empty() && input_directory[input_directory.size() - 1]
                                    != '/') {
    path << '/';
  }
  path << 'a' << sample_prefix << '.' << std::setfill('0') << std::setw(4)
       << file_index << ".root";
  return path.str();
}

std::vector<std::string> BuildInputFileList(
    const std::string& input_directory,
    const RunSampleDefinition& sample) {
  if (sample.prefix.empty()) {
    throw std::invalid_argument("input sample prefix must not be empty");
  }
  if (sample.first_file > sample.last_file) {
    throw std::invalid_argument("invalid input sample file range");
  }
  std::vector<std::string> files;
  files.reserve(static_cast<std::size_t>(sample.last_file - sample.first_file +
                                         1));
  for (int file_index = sample.first_file; file_index <= sample.last_file;
       ++file_index) {
    files.push_back(
        FormatInputFilename(input_directory, sample.prefix, file_index));
  }
  return files;
}

std::string HighGainEnergyBranchName(unsigned int channel) {
  std::ostringstream name;
  name << "GAMMA" << channel + 1 << "_HIGH_E";
  return name.str();
}

std::string TimeBranchName(unsigned int channel) {
  std::ostringstream name;
  name << "GAMMA" << channel + 1 << "_T";
  return name.str();
}

std::string TimeSelection(const SourceBackgroundDefinition& definition,
                          unsigned int channel) {
  std::ostringstream selection;
  const std::string branch = TimeBranchName(channel);
  selection << branch << '>' << definition.time_min_exclusive << "&&"
            << branch << '<' << definition.time_max_exclusive;
  return selection.str();
}

std::string SourceHistogramName(unsigned int channel) {
  return FormatChannelObjectName("h_src_XE_", channel);
}

std::string BackgroundHistogramName(unsigned int channel) {
  return FormatChannelObjectName("h_bkg_XE_", channel);
}

std::string NetHistogramName(unsigned int channel) {
  return FormatChannelObjectName("h_nobkg_XE_", channel);
}

SourceBackgroundSummary BuildSourceBackgroundSpectra(
    const SourceBackgroundDefinition& definition,
    const std::string& input_directory,
    const std::string& output_root_file,
    const std::string& report_file,
    bool overwrite) {
  ValidateSourceBackgroundDefinition(definition);
  if (!report_file.empty() && report_file == output_root_file) {
    throw std::invalid_argument(
        "ROOT output and run report must use different paths");
  }
  RequireNewOutputPath(output_root_file, overwrite);
  if (!report_file.empty()) {
    RequireNewOutputPath(report_file, overwrite);
  }

  SourceBackgroundSummary summary;
  summary.source_files = BuildInputFileList(input_directory, definition.source);
  summary.background_files =
      BuildInputFileList(input_directory, definition.background);

  RequireReadableFiles(summary.source_files, "source");
  RequireReadableFiles(summary.background_files, "background");
  RequireInputSchemas(summary.source_files, definition, "source");
  RequireInputSchemas(summary.background_files, definition, "background");

  detail::EnsureOutputParentDirectories({output_root_file, report_file});

  TChain source_chain(definition.tree_name.c_str(), "");
  TChain background_chain(definition.tree_name.c_str(), "");
  AddFiles(source_chain, summary.source_files, "source");
  AddFiles(background_chain, summary.background_files, "background");
  RequireBranches(source_chain, definition, "source");
  RequireBranches(background_chain, definition, "background");

  summary.source_tree_entries = source_chain.GetEntries();
  summary.background_tree_entries = background_chain.GetEntries();

  TFile output(output_root_file.c_str(), overwrite ? "RECREATE" : "CREATE");
  if (output.IsZombie()) {
    throw std::runtime_error("cannot create ROOT output: " + output_root_file);
  }

  summary.channels.reserve(definition.channel_count);
  for (unsigned int channel = 0; channel < definition.channel_count;
       ++channel) {
    output.cd();

    const std::string source_name = SourceHistogramName(channel);
    const std::string background_name = BackgroundHistogramName(channel);
    const std::string net_name = NetHistogramName(channel);
    const std::string energy_branch = HighGainEnergyBranchName(channel);
    const std::string time_selection = TimeSelection(definition, channel);

    TH1I* source_histogram =
        new TH1I(source_name.c_str(),
                 (std::string("Source Gamma_XE[") +
                  FormatChannelObjectName("", channel) + "]")
                     .c_str(),
                 definition.adc_bin_count,
                 definition.adc_min,
                 definition.adc_max);
    const std::string source_draw = energy_branch + ">>" + source_name;
    const Long64_t source_selected = source_chain.Draw(
        source_draw.c_str(), time_selection.c_str(), "goff");
    if (source_selected < 0) {
      throw std::runtime_error("source TTree::Draw failed for channel " +
                               FormatChannelObjectName("", channel));
    }
    output.WriteTObject(source_histogram);

    TH1I* background_histogram =
        new TH1I(background_name.c_str(),
                 (std::string("Bkg Gamma_XE[") +
                  FormatChannelObjectName("", channel) + "]")
                     .c_str(),
                 definition.adc_bin_count,
                 definition.adc_min,
                 definition.adc_max);
    const std::string background_draw = energy_branch + ">>" + background_name;
    const Long64_t background_selected = background_chain.Draw(
        background_draw.c_str(), time_selection.c_str(), "goff");
    if (background_selected < 0) {
      throw std::runtime_error("background TTree::Draw failed for channel " +
                               FormatChannelObjectName("", channel));
    }
    output.WriteTObject(background_histogram);

    TH1D* net_histogram = new TH1D(net_name.c_str(), "",
                                   definition.adc_bin_count,
                                   definition.adc_min,
                                   definition.adc_max);
    source_histogram->Sumw2();
    background_histogram->Sumw2();
    net_histogram->Add(source_histogram,
                       background_histogram,
                       1.0 / definition.source.live_time_seconds,
                       -1.0 / definition.background.live_time_seconds);
    output.WriteTObject(net_histogram);

    ChannelSpectrumSummary channel_summary;
    channel_summary.channel = channel;
    channel_summary.source_selected_entries = source_selected;
    channel_summary.background_selected_entries = background_selected;
    channel_summary.source_all_bins = AllBins(*source_histogram);
    channel_summary.background_all_bins = AllBins(*background_histogram);
    channel_summary.net_rate_all_bins = AllBins(*net_histogram);
    summary.channels.push_back(channel_summary);
  }

  output.Close();
  if (!report_file.empty()) {
    WriteSourceBackgroundReport(
        report_file, definition, summary, output_root_file, overwrite);
  }
  return summary;
}

void WriteSourceBackgroundReport(
    const std::string& report_file,
    const SourceBackgroundDefinition& definition,
    const SourceBackgroundSummary& summary,
    const std::string& output_root_file,
    bool overwrite) {
  RequireNewOutputPath(report_file, overwrite);
  std::ofstream report(report_file.c_str(),
                       overwrite ? std::ios::trunc : std::ios::out);
  if (!report) {
    throw std::runtime_error("cannot create run report: " + report_file);
  }

  report << std::setprecision(17);
  report << "record\tfield_1\tfield_2\tfield_3\tfield_4\tfield_5\tfield_6\n";
  report << "config\tname\t" << definition.name << "\t\t\t\t\n";
  report << "config\ttree_name\t" << definition.tree_name
         << "\t\t\t\t\n";
  report << "config\toutput_root\t" << output_root_file
         << "\t\t\t\t\n";
  report << "config\tsource_live_time_seconds\t"
         << definition.source.live_time_seconds << "\t\t\t\t\n";
  report << "config\tbackground_live_time_seconds\t"
         << definition.background.live_time_seconds << "\t\t\t\t\n";
  report << "config\ttime_window_exclusive\t"
         << definition.time_min_exclusive << '\t'
         << definition.time_max_exclusive << "\t\t\t\n";
  report << "config\tadc_histogram\t" << definition.adc_bin_count << '\t'
         << definition.adc_min << '\t' << definition.adc_max << "\t\t\n";
  report << "entries\tsource_tree\t" << summary.source_tree_entries
         << "\t\t\t\t\n";
  report << "entries\tbackground_tree\t"
         << summary.background_tree_entries << "\t\t\t\t\n";

  for (std::size_t index = 0; index < summary.source_files.size(); ++index) {
    report << "input\tsource\t" << index << '\t'
           << summary.source_files[index] << "\t\t\t\n";
  }
  for (std::size_t index = 0; index < summary.background_files.size();
       ++index) {
    report << "input\tbackground\t" << index << '\t'
           << summary.background_files[index] << "\t\t\t\n";
  }
  for (std::vector<ChannelSpectrumSummary>::const_iterator channel =
           summary.channels.begin();
       channel != summary.channels.end(); ++channel) {
    report << "channel\t" << channel->channel << '\t'
           << channel->source_selected_entries << '\t'
           << channel->background_selected_entries << '\t'
           << channel->source_all_bins << '\t'
           << channel->background_all_bins << '\t'
           << channel->net_rate_all_bins << '\n';
  }
}

}  // namespace cshine_gamma
