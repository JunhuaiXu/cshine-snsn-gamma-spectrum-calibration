#include "source_background_spectra.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

struct Options {
  std::string input_directory;
  std::string output_root_file;
  std::string report_file;
  bool overwrite;
  bool print_config;

  Options() : overwrite(false), print_config(false) {}
};

void PrintUsage(const char* program) {
  std::cout
      << "Usage:\n  " << program
      << " --input-dir DIR --output FILE [--report FILE] [--overwrite]\n"
      << "  " << program << " --print-config\n\n"
      << "The input directory must contain the exact central-0308 files "
         "named\n"
      << "a20240308_ThnatCo60.0000.root through .0020.root and\n"
      << "a20240308_BKG_ALLOR.0000.root through .0023.root.\n";
}

Options ParseOptions(int argc, char** argv) {
  Options options;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--input-dir" || argument == "--output" ||
        argument == "--report") {
      if (index + 1 >= argc) {
        throw std::invalid_argument("missing value after " + argument);
      }
      const std::string value = argv[++index];
      if (argument == "--input-dir") {
        options.input_directory = value;
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
    const cshine_gamma::SourceBackgroundDefinition& definition) {
  std::cout << "name=" << definition.name << '\n'
            << "tree_name=" << definition.tree_name << '\n'
            << "source_prefix=" << definition.source.prefix << '\n'
            << "source_files=" << definition.source.first_file << ".."
            << definition.source.last_file << '\n'
            << "source_live_time_seconds="
            << definition.source.live_time_seconds << '\n'
            << "background_prefix=" << definition.background.prefix << '\n'
            << "background_files=" << definition.background.first_file
            << ".." << definition.background.last_file << '\n'
            << "background_live_time_seconds="
            << definition.background.live_time_seconds << '\n'
            << "channels=" << definition.channel_count << '\n'
            << "adc_histogram=" << definition.adc_bin_count << ','
            << definition.adc_min << ',' << definition.adc_max << '\n'
            << "time_window_exclusive=" << definition.time_min_exclusive
            << ',' << definition.time_max_exclusive << '\n';
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const Options options = ParseOptions(argc, argv);
    const cshine_gamma::SourceBackgroundDefinition definition =
        cshine_gamma::Central0308SourceBackgroundDefinition();

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
    const cshine_gamma::SourceBackgroundSummary summary =
        cshine_gamma::BuildSourceBackgroundSpectra(
            definition,
            options.input_directory,
            options.output_root_file,
            report_file,
            options.overwrite);

    std::cout << "output=" << options.output_root_file << '\n'
              << "report=" << report_file << '\n'
              << "source_files=" << summary.source_files.size() << '\n'
              << "background_files=" << summary.background_files.size()
              << '\n'
              << "channels=" << summary.channels.size() << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "build_source_spectra: " << error.what() << '\n';
    return 1;
  }
}
