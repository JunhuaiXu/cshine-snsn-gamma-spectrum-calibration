#include "neighbor_time_diagnostics.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Options {
  std::vector<std::string> inputs;
  std::string output_root_file;
  std::string report_file;
  bool overwrite;
  bool print_config;
  Options() : overwrite(false), print_config(false) {}
};

void PrintUsage(const char* program) {
  std::cout << "Usage:\n  " << program
            << " --input FILE_OR_PATTERN [--input FILE_OR_PATTERN ...]"
               " --output FILE [--report FILE] [--overwrite]\n"
            << "  " << program << " --print-config\n";
}

Options ParseOptions(int argc, char** argv) {
  Options options;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--input" || argument == "--output" ||
        argument == "--report") {
      if (index + 1 >= argc) {
        throw std::invalid_argument("missing value after " + argument);
      }
      const std::string value = argv[++index];
      if (argument == "--input") {
        options.inputs.push_back(value);
      } else if (argument == "--output") {
        options.output_root_file = value;
      } else {
        options.report_file = value;
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

void PrintDefinition(const cshine_gamma::NeighborTimeDefinition& definition) {
  std::cout << "name=" << definition.name << '\n'
            << "input_tree=" << definition.input_tree_name << '\n'
            << "crystals=" << definition.first_crystal << ','
            << definition.second_crystal << '\n'
            << "energy_selection=GammaEnergy[5]+GammaEnergy[6]>=30 MeV\n"
            << "time_histograms=100,-500,500 ns\n"
            << "difference_histograms=100,-200,200 ns\n"
            << "objects=h2_all,h2_cut,hh_diff,h1,h3,h4\n"
            << "display_scaling=peak_ratio_h1_over_hh_diff_not_stored\n";
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const Options options = ParseOptions(argc, argv);
    const cshine_gamma::NeighborTimeDefinition definition =
        cshine_gamma::HistoricalCsI05CsI06NeighborTimeDefinition();
    if (options.print_config) {
      PrintDefinition(definition);
      return 0;
    }
    if (options.inputs.empty() || options.output_root_file.empty()) {
      PrintUsage(argv[0]);
      return 2;
    }
    const std::string report_file =
        options.report_file.empty() ? options.output_root_file + ".run.tsv"
                                    : options.report_file;
    const cshine_gamma::NeighborTimeSummary summary =
        cshine_gamma::BuildNeighborTimeDiagnostics(
            definition, options.inputs, options.output_root_file, report_file,
            options.overwrite);
    std::cout << "output=" << options.output_root_file << '\n'
              << "report=" << report_file << '\n'
              << "input_files=" << summary.resolved_input_files.size() << '\n'
              << "tree_entries=" << summary.tree_entries << '\n'
              << "historical_peak_scale=" << summary.historical_peak_scale
              << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "build_neighbor_time_diagnostics: " << error.what() << '\n';
    return 1;
  }
}
