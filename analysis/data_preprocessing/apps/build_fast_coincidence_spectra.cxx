#include "fast_coincidence_spectra.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Options {
  std::vector<std::string> inputs;
  std::string signal_output;
  std::string random_output;
  std::string report;
  bool overwrite = false;
  bool print_config = false;
};

void PrintUsage(const char* program) {
  std::cout << "Usage:\n  " << program
            << " --input FILE [--input FILE ...] --signal-output FILE"
               " --random-output FILE [--report FILE] [--overwrite]\n  "
            << program << " --print-config\n";
}

Options ParseOptions(int argc, char** argv) {
  Options options;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--input" || argument == "--signal-output" ||
        argument == "--random-output" || argument == "--report") {
      if (index + 1 >= argc) {
        throw std::invalid_argument("missing value after " + argument);
      }
      const std::string value = argv[++index];
      if (argument == "--input") options.inputs.push_back(value);
      else if (argument == "--signal-output") options.signal_output = value;
      else if (argument == "--random-output") options.random_output = value;
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
  const cshine_gamma::FastCoincidenceDefinition definition =
      cshine_gamma::HistoricalFastCoincidenceDefinition();
  std::cout << "name=" << definition.name << '\n'
            << "input_tree=" << definition.input_tree_name << '\n'
            << "ssd_m2_exclusion=strict_835_lt_TDC_Gamma_Trig_list[18]_lt_850\n"
            << "signal_time_window_ns=inclusive_-350:-50\n"
            << "random_time_window_ns=inclusive_50:350\n"
            << "candidate_policy=central_plus_side_with_three-face-veto-silence\n"
            << "energy_binning=1000:0:200_MeV\n"
            << "output_objects=h_central_E_M1,h_side_E_M1,h_total_E_M1\n";
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const Options options = ParseOptions(argc, argv);
    if (options.print_config) {
      PrintDefinition();
      return 0;
    }
    if (options.inputs.empty() || options.signal_output.empty() ||
        options.random_output.empty()) {
      PrintUsage(argv[0]);
      return 2;
    }
    const std::string report = options.report.empty()
                                   ? options.signal_output + ".run.tsv"
                                   : options.report;
    const cshine_gamma::FastCoincidenceSummary summary =
        cshine_gamma::BuildFastCoincidenceSpectra(
            cshine_gamma::HistoricalFastCoincidenceDefinition(),
            options.inputs, options.signal_output, options.random_output,
            report, options.overwrite);
    std::cout << "input_entries=" << summary.input_entries << '\n'
              << "excluded_ssd_m2_entries="
              << summary.excluded_ssd_m2_entries << '\n'
              << "signal_output=" << options.signal_output << '\n'
              << "random_output=" << options.random_output << '\n'
              << "report=" << report << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "build_fast_coincidence_spectra: " << error.what() << '\n';
    return 1;
  }
}
