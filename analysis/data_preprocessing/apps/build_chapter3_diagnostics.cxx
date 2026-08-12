#include "chapter3_diagnostics.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Options {
  std::vector<std::string> inputs;
  std::string output;
  std::string report;
  std::string sample_role;
  bool overwrite = false;
  bool print_config = false;
};

void PrintUsage(const char* program) {
  std::cout << "Usage:\n  " << program
            << " --input M8.root [--input M8.root ...] --output FILE"
               " --sample-role beam-on|beam-off [--report FILE] [--overwrite]\n"
            << "  " << program << " --print-config\n";
}

Options ParseOptions(int argc, char** argv) {
  Options options;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--input" || argument == "--output" ||
        argument == "--report" || argument == "--sample-role") {
      if (index + 1 >= argc) {
        throw std::invalid_argument("missing value after " + argument);
      }
      const std::string value = argv[++index];
      if (argument == "--input") {
        options.inputs.push_back(value);
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
  const cshine_gamma::Chapter3DiagnosticsDefinition definition =
      cshine_gamma::HistoricalChapter3DiagnosticsDefinition();
  std::cout << "name=" << definition.name << '\n'
            << "input_tree=" << definition.input_tree_name << '\n'
            << "reconstruction=stored_recon_result\n"
            << "crystal_pitch_cm=" << definition.crystal_pitch_cm << '\n'
            << "main_central=5,6,9,10;veto=none\n"
            << "main_side=4,7,8,11,13,14;count_veto=0\n"
            << "multiplicity=all15;veto=none;threshold_mev=>"
            << definition.high_energy_threshold_mev << '\n';
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const Options options = ParseOptions(argc, argv);
    if (options.print_config) {
      PrintDefinition();
      return 0;
    }
    if (options.inputs.empty() || options.output.empty() ||
        options.sample_role.empty()) {
      PrintUsage(argv[0]);
      return 2;
    }
    const std::string report =
        options.report.empty() ? options.output + ".run.tsv" : options.report;
    const cshine_gamma::Chapter3DiagnosticsSummary summary =
        cshine_gamma::BuildChapter3Diagnostics(
            cshine_gamma::HistoricalChapter3DiagnosticsDefinition(),
            options.inputs, options.output, report, options.sample_role,
            options.overwrite);
    std::cout << "sample_role=" << summary.sample_role << '\n'
              << "input_files=" << summary.resolved_input_files.size() << '\n'
              << "input_entries=" << summary.input_entries << '\n'
              << "main_candidates=" << summary.main_candidate_count << '\n'
              << "all15_candidates=" << summary.all15_candidate_count << '\n'
              << "output=" << options.output << '\n'
              << "report=" << report << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "build_chapter3_diagnostics: " << error.what() << '\n';
    return 1;
  }
}
