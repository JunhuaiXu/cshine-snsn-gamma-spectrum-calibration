#include "gain_relation.h"

#include <TROOT.h>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

struct Options {
  std::string input_directory;
  std::string output_root_file;
  std::string parameter_file;
  std::string report_file;
  std::string canvas_pdf_file;
  std::string canvas_png_file;
  bool overwrite;
  bool print_config;

  Options() : overwrite(false), print_config(false) {}
};

void PrintUsage(const char* program) {
  std::cout
      << "Usage:\n  " << program
      << " --input-dir DIR --output FILE [--parameters FILE] [--report FILE]"
         "\n      [--canvas-pdf FILE] [--canvas-png FILE] [--overwrite]\n"
      << "  " << program << " --print-config\n\n"
      << "The input directory must contain the 105 central-0308 files named\n"
      << "a20240308_SnSn_GOAL_ALLCOIN.0000.root through .0104.root.\n";
}

Options ParseOptions(int argc, char** argv) {
  Options options;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--input-dir" || argument == "--output" ||
        argument == "--parameters" || argument == "--report" ||
        argument == "--canvas-pdf" || argument == "--canvas-png") {
      if (index + 1 >= argc) {
        throw std::invalid_argument("missing value after " + argument);
      }
      const std::string value = argv[++index];
      if (argument == "--input-dir") {
        options.input_directory = value;
      } else if (argument == "--output") {
        options.output_root_file = value;
      } else if (argument == "--parameters") {
        options.parameter_file = value;
      } else if (argument == "--report") {
        options.report_file = value;
      } else if (argument == "--canvas-pdf") {
        options.canvas_pdf_file = value;
      } else {
        options.canvas_png_file = value;
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

void PrintDefinition(const cshine_gamma::GainRelationDefinition& definition) {
  std::cout << "name=" << definition.name << '\n'
            << "tree_name=" << definition.tree_name << '\n'
            << "input_prefix=" << definition.input_prefix << '\n'
            << "files=" << definition.first_file << ".."
            << definition.last_file << '\n'
            << "channels=" << definition.channel_count << '\n'
            << "low_gain_selection_exclusive="
            << definition.low_gain_min_exclusive << ','
            << definition.low_gain_max_exclusive << '\n'
            << "high_gain_fit_range=" << definition.fit_high_gain_min << ','
            << definition.fit_high_gain_max << '\n'
            << "fit_model=[a0]+[a1]*x\n"
            << "slope_limits=" << definition.slope_min << ','
            << definition.slope_max << '\n'
            << "output_object=" << definition.output_object_name << ':'
            << definition.output_object_title << '\n'
            << "canvas=" << definition.canvas_name << ':'
            << definition.canvas_title << '\n';
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const Options options = ParseOptions(argc, argv);
    const cshine_gamma::GainRelationDefinition definition =
        cshine_gamma::Central0308GainRelationDefinition();
    if (options.print_config) {
      PrintDefinition(definition);
      return 0;
    }
    if (options.input_directory.empty() || options.output_root_file.empty()) {
      PrintUsage(argv[0]);
      return 2;
    }

    const std::string parameter_file =
        options.parameter_file.empty()
            ? options.output_root_file + ".parameters.txt"
            : options.parameter_file;
    const std::string report_file =
        options.report_file.empty() ? options.output_root_file + ".run.tsv"
                                    : options.report_file;

    gROOT->SetBatch(kTRUE);
    const cshine_gamma::GainRelationSummary summary =
        cshine_gamma::FitGainRelation(definition,
                                      options.input_directory,
                                      options.output_root_file,
                                      parameter_file,
                                      report_file,
                                      options.canvas_pdf_file,
                                      options.canvas_png_file,
                                      options.overwrite);
    std::cout << "output=" << options.output_root_file << '\n'
              << "parameters=" << parameter_file << '\n'
              << "report=" << report_file << '\n'
              << "input_files=" << summary.input_files.size() << '\n'
              << "tree_entries=" << summary.tree_entries << '\n'
              << "channels=" << summary.channels.size() << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "fit_gain_relation: " << error.what() << '\n';
    return 1;
  }
}
