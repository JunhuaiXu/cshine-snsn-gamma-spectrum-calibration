#include "trigger_diagnostics.h"

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
  bool overwrite = false;
  bool print_config = false;
};

void PrintUsage(const char* program) {
  std::cout << "Usage:\n  " << program
            << " --input M8.root [--input M8.root ...] --output FILE"
               " [--report FILE] [--overwrite]\n"
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
        options.output = value;
      } else {
        options.report = value;
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
  const cshine_gamma::TriggerDiagnosticsDefinition definition =
      cshine_gamma::HistoricalTriggerDiagnosticsDefinition();
  std::cout << "name=" << definition.name << '\n'
            << "input_tree=" << definition.input_tree_name << '\n'
            << "monitor_window=100<tdc<4000\n"
            << "condition_window=100<=tdc<=4000\n"
            << "conditioned_indices=17,18,19,20,22\n"
            << "historical_selection=central:veto0;side:any\n"
            << "reviewed_selection=central:any;side:veto0\n";
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const Options options = ParseOptions(argc, argv);
    if (options.print_config) {
      PrintDefinition();
      return 0;
    }
    if (options.inputs.empty() || options.output.empty()) {
      PrintUsage(argv[0]);
      return 2;
    }
    const std::string report =
        options.report.empty() ? options.output + ".run.tsv" : options.report;
    const cshine_gamma::TriggerDiagnosticsSummary summary =
        cshine_gamma::BuildTriggerDiagnostics(
            cshine_gamma::HistoricalTriggerDiagnosticsDefinition(),
            options.inputs, options.output, report, options.overwrite);
    std::cout << "input_files=" << summary.resolved_input_files.size() << '\n'
              << "input_entries=" << summary.input_entries << '\n'
              << "output=" << options.output << '\n'
              << "report=" << report << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "build_trigger_diagnostics: " << error.what() << '\n';
    return 1;
  }
}
