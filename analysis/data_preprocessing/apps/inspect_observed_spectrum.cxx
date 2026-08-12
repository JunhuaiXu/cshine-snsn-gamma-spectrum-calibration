#include "observed_spectrum.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

struct Options {
  std::string input;
  std::string report;
  bool overwrite = false;
  bool print_config = false;
};

void PrintUsage(const char* program) {
  std::cout << "Usage:\n  " << program
            << " --input FILE --report FILE [--overwrite]\n  "
            << program << " --print-config\n";
}

Options ParseOptions(int argc, char** argv) {
  Options options;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--input" || argument == "--report") {
      if (index + 1 >= argc) {
        throw std::invalid_argument("missing value after " + argument);
      }
      const std::string value = argv[++index];
      if (argument == "--input") options.input = value;
      else options.report = value;
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
  const cshine_gamma::ObservedSpectrumInterfaceDefinition definition =
      cshine_gamma::HistoricalObservedSpectrumInterfaceDefinition();
  std::cout << "object_name=" << definition.object_name << '\n'
            << "object_class=" << definition.object_class << '\n'
            << "binning=" << definition.bins << ':'
            << definition.energy_min_mev << ':'
            << definition.energy_max_mev << "_MeV\n"
            << "bin_width_MeV=" << definition.bin_width_mev << '\n'
            << "energy_frame=" << definition.energy_frame << '\n'
            << "value_semantics=" << definition.value_semantics << '\n'
            << "uncertainty_semantics="
            << definition.uncertainty_semantics << '\n';
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const Options options = ParseOptions(argc, argv);
    if (options.print_config) {
      PrintDefinition();
      return 0;
    }
    if (options.input.empty() || options.report.empty()) {
      PrintUsage(argv[0]);
      return 2;
    }
    const cshine_gamma::ObservedSpectrumInterfaceSummary summary =
        cshine_gamma::InspectObservedSpectrumInterface(
            cshine_gamma::HistoricalObservedSpectrumInterfaceDefinition(),
            options.input, options.report, options.overwrite);
    std::cout << "object_class=" << summary.object_class << '\n'
              << "bins=" << summary.bins << '\n'
              << "negative_regular_bins=" << summary.negative_regular_bins
              << '\n'
              << "report=" << options.report << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "inspect_observed_spectrum: " << error.what() << '\n';
    return 1;
  }
}
