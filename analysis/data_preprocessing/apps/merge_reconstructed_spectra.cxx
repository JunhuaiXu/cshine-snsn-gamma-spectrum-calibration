#include "reconstructed_spectrum_merge.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Options {
  std::vector<std::string> inputs;
  std::string per_crystal_output;
  std::string output;
  std::string report;
  std::string sample_role;
  bool overwrite = false;
  bool print_config = false;
};

void PrintUsage(const char* program) {
  std::cout << "Usage:\n  " << program
            << " --input M8.root [--input M8.root ...]"
               " --per-crystal-output FILE --output FILE"
               " --sample-role beam-on|beam-off [--report FILE] [--overwrite]\n"
            << "  " << program << " --print-config\n";
}

Options ParseOptions(int argc, char** argv) {
  Options options;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--input" || argument == "--per-crystal-output" ||
        argument == "--output" || argument == "--report" ||
        argument == "--sample-role") {
      if (index + 1 >= argc) {
        throw std::invalid_argument("missing value after " + argument);
      }
      const std::string value = argv[++index];
      if (argument == "--input") {
        options.inputs.push_back(value);
      } else if (argument == "--per-crystal-output") {
        options.per_crystal_output = value;
      } else if (argument == "--output") {
        options.output = value;
      } else if (argument == "--report") {
        options.report = value;
      } else {
        options.sample_role = value;
      }
    } else if (argument == "--overwrite") {
      options.overwrite = true;
    } else if (argument == "--print-config") {
      options.print_config = true;
    } else if (argument == "--help" || argument == "-h") {
      PrintUsage(argv[0]);
      std::exit(0);
    } else {
      throw std::invalid_argument("unknown argument: " + argument);
    }
  }
  return options;
}

void PrintDefinition() {
  const cshine_gamma::ReconstructedSpectrumMergeDefinition definition =
      cshine_gamma::HistoricalReconstructedSpectrumMergeDefinition();
  std::cout << "name=" << definition.name << '\n'
            << "crystal_count=" << definition.crystal_count << '\n'
            << "histogram=TH1I;1000;0;200_MeV\n"
            << "central_crystals=5,6,9,10;input=h_recon\n"
            << "side_crystals=4,7,8,11,13,14;input=h_recon_veto\n"
            << "per_crystal_objects=h_eDep_0..14,h_recon_0..14,"
               "h_recon_veto_0..14,"
               "h_recon_vetoed_0..14\n"
            << "merged_objects=h_central_E_M1,h_side_E_M1,h_total_E_M1,c1,"
               "h_rate\n";
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const Options options = ParseOptions(argc, argv);
    if (options.print_config) {
      PrintDefinition();
      return 0;
    }
    if (options.inputs.empty() || options.per_crystal_output.empty() ||
        options.output.empty() || options.sample_role.empty()) {
      PrintUsage(argv[0]);
      return 2;
    }
    const std::string report =
        options.report.empty() ? options.output + ".run.tsv" : options.report;
    const cshine_gamma::ReconstructedSpectrumMergeSummary summary =
        cshine_gamma::MergeReconstructedSpectra(
            cshine_gamma::HistoricalReconstructedSpectrumMergeDefinition(),
            options.inputs, options.per_crystal_output, options.output, report,
            options.sample_role, options.overwrite);
    std::cout << "sample_role=" << summary.sample_role << '\n'
              << "input_files=" << summary.input_files.size() << '\n'
              << "per_crystal_output=" << options.per_crystal_output << '\n'
              << "output=" << options.output << '\n'
              << "report=" << report << '\n'
              << "central_entries=" << summary.central_entries << '\n'
              << "side_veto_silent_entries="
              << summary.side_veto_silent_entries << '\n'
              << "total_entries=" << summary.total_entries << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "merge_reconstructed_spectra: " << error.what() << '\n';
    return 1;
  }
}
