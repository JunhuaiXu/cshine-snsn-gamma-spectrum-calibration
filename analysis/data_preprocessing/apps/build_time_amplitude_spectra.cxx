#include "time_amplitude_spectra.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

struct Options {
  std::string input_directory;
  std::string output_root_file;
  std::string report_file;
  std::string mode;
  unsigned int threads;
  bool overwrite;
  bool print_config;

  Options()
      : mode("original"), threads(12), overwrite(false), print_config(false) {}
};

void PrintUsage(const char* program) {
  std::cout
      << "Usage:\n  " << program
      << " --input-dir DIR --output FILE [--report FILE]\n"
      << "      [--mode original|historical-corrected] [--threads N]"
         " [--overwrite]\n"
      << "  " << program
      << " --print-config [--mode original|historical-corrected]"
         " [--threads N]\n\n"
      << "The historical-corrected mode preserves the implicit conversion in "
         "time_cali.C and is diagnostic only.\n";
}

unsigned int ParsePositiveUnsigned(const std::string& text,
                                   const std::string& option) {
  char* end = nullptr;
  const unsigned long value = std::strtoul(text.c_str(), &end, 10);
  if (end == text.c_str() || *end != '\0' || value == 0 ||
      value > std::numeric_limits<unsigned int>::max()) {
    throw std::invalid_argument("invalid positive integer for " + option);
  }
  return static_cast<unsigned int>(value);
}

Options ParseOptions(int argc, char** argv) {
  Options options;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--input-dir" || argument == "--output" ||
        argument == "--report" || argument == "--mode" ||
        argument == "--threads") {
      if (index + 1 >= argc) {
        throw std::invalid_argument("missing value after " + argument);
      }
      const std::string value = argv[++index];
      if (argument == "--input-dir") {
        options.input_directory = value;
      } else if (argument == "--output") {
        options.output_root_file = value;
      } else if (argument == "--report") {
        options.report_file = value;
      } else if (argument == "--mode") {
        options.mode = value;
      } else {
        options.threads = ParsePositiveUnsigned(value, argument);
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

cshine_gamma::TimeAmplitudeDefinition DefinitionForOptions(
    const Options& options) {
  cshine_gamma::TimeAmplitudeDefinition definition;
  if (options.mode == "original") {
    definition = cshine_gamma::CentralOriginalTimeAmplitudeDefinition();
  } else if (options.mode == "historical-corrected") {
    definition =
        cshine_gamma::CentralHistoricalCorrectedTimeAmplitudeDefinition();
  } else {
    throw std::invalid_argument("unknown mode: " + options.mode);
  }
  definition.thread_count = options.threads;
  return definition;
}

void PrintDefinition(const cshine_gamma::TimeAmplitudeDefinition& definition) {
  std::cout << "name=" << definition.name << '\n'
            << "mode=" << cshine_gamma::TimeAmplitudeModeName(definition.mode)
            << '\n'
            << "tree_name=" << definition.tree_name << '\n'
            << "input_patterns=" << definition.input_patterns.size() << '\n'
            << "channels=" << definition.channel_count << '\n'
            << "t0_channels=" << definition.t0_count << '\n'
            << "threads=" << definition.thread_count << '\n'
            << "tdc_inclusive=" << definition.tdc_min_inclusive << ','
            << definition.tdc_max_inclusive << '\n'
            << "time_histogram=" << definition.time_bin_count << ','
            << definition.time_min_ns << ',' << definition.time_max_ns << '\n'
            << "adc_histogram=" << definition.adc_bin_count << ','
            << definition.adc_min_channel << ','
            << definition.adc_max_channel << '\n';
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const Options options = ParseOptions(argc, argv);
    const cshine_gamma::TimeAmplitudeDefinition definition =
        DefinitionForOptions(options);
    if (options.print_config) {
      PrintDefinition(definition);
      return 0;
    }
    if (options.input_directory.empty() || options.output_root_file.empty()) {
      PrintUsage(argv[0]);
      return 2;
    }
    const std::string report_file =
        options.report_file.empty()
            ? options.output_root_file + ".run.tsv"
            : options.report_file;
    const cshine_gamma::TimeAmplitudeSummary summary =
        cshine_gamma::BuildTimeAmplitudeSpectra(
            definition, options.input_directory, options.output_root_file,
            report_file, options.overwrite);
    std::cout << "output=" << options.output_root_file << '\n'
              << "report=" << report_file << '\n'
              << "mode="
              << cshine_gamma::TimeAmplitudeModeName(definition.mode) << '\n'
              << "input_files=" << summary.input_file_count << '\n'
              << "tree_entries=" << summary.tree_entries << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "build_time_amplitude_spectra: " << error.what() << '\n';
    return 1;
  }
}
