#include "calibrated_event_tree.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Options {
  std::vector<std::string> inputs;
  std::string calibration_file;
  std::string output_root_file;
  std::string report_file;
  bool overwrite;
  bool print_config;

  Options() : overwrite(false), print_config(false) {}
};

void PrintUsage(const char* program) {
  std::cout
      << "Usage:\n  " << program
      << " --input FILE_OR_PATTERN [--input FILE_OR_PATTERN ...]"
         " --calibration FILE --output FILE [--report FILE] [--overwrite]\n"
      << "  " << program << " --print-config\n";
}

Options ParseOptions(int argc, char** argv) {
  Options options;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--input" || argument == "--calibration" ||
        argument == "--output" || argument == "--report") {
      if (index + 1 >= argc) {
        throw std::invalid_argument("missing value after " + argument);
      }
      const std::string value = argv[++index];
      if (argument == "--input") {
        options.inputs.push_back(value);
      } else if (argument == "--calibration") {
        options.calibration_file = value;
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

void PrintDefinition(
    const cshine_gamma::CalibratedEventDefinition& definition) {
  std::cout << "name=" << definition.name << '\n'
            << "input_tree=" << definition.input_tree_name << '\n'
            << "output_tree=" << definition.output_tree_name << ':'
            << definition.output_tree_title << '\n'
            << "calibration_object=" << definition.calibration_object_name
            << '\n'
            << "crystals=" << definition.crystal_count << '\n'
            << "adc_storage=" << definition.adc_storage_count << '\n'
            << "tdc_storage=" << definition.tdc_storage_count << '\n'
            << "t0_storage=" << definition.t0_count << '\n'
            << "veto_storage=" << definition.veto_count << '\n'
            << "time_validity=strict_100_lt_tdc_lt_4000\n"
            << "t0_role=retained_raw_only\n"
            << "veto_role=retained_raw_only_for_M8\n"
            << "deferred=GammaEnergyCorrected,recon_result,count_veto\n";
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const Options options = ParseOptions(argc, argv);
    const cshine_gamma::CalibratedEventDefinition definition =
        cshine_gamma::Central0308CalibratedEventDefinition();
    if (options.print_config) {
      PrintDefinition(definition);
      return 0;
    }
    if (options.inputs.empty() || options.calibration_file.empty() ||
        options.output_root_file.empty()) {
      PrintUsage(argv[0]);
      return 2;
    }
    const std::string report_file =
        options.report_file.empty() ? options.output_root_file + ".run.tsv"
                                    : options.report_file;
    const cshine_gamma::CalibratedEventSummary summary =
        cshine_gamma::BuildCalibratedEventTree(
            definition, options.inputs, options.calibration_file,
            options.output_root_file, report_file, options.overwrite);
    std::cout << "output=" << options.output_root_file << '\n'
              << "report=" << report_file << '\n'
              << "input_files=" << summary.resolved_input_files.size() << '\n'
              << "input_entries=" << summary.input_entries << '\n'
              << "output_entries=" << summary.output_entries << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "build_calibrated_event_tree: " << error.what() << '\n';
    return 1;
  }
}
