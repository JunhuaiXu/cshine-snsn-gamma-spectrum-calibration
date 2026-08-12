#include "reconstructed_event_tree.h"

#include "shower_reconstruction.h"

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
  const cshine_gamma::ReconstructedEventDefinition tree =
      cshine_gamma::HistoricalReconstructedEventDefinition();
  const cshine_gamma::ShowerReconstructionDefinition shower =
      cshine_gamma::HistoricalShowerReconstructionDefinition();
  std::cout << "name=" << tree.name << '\n'
            << "input_tree=" << tree.input_tree_name << '\n'
            << "output_tree=" << tree.output_tree_name << '\n'
            << "energy_threshold_mev="
            << shower.crystal_energy_threshold_mev << '\n'
            << "neighbor_time_ns=" << shower.neighbor_time_window_ns << '\n'
            << "separate_core_time_ns=" << shower.separate_core_time_ns << '\n'
            << "central_cores=5,6,9,10\n"
            << "side_cores=4,7,8,11,13,14\n"
            << "lower_edge_without_veto=1,2\n"
            << "corner_crystals=0,3,12\n"
            << "detector_histograms=h_eDep_0..14;TH1I;1000;0;100_MeV\n"
            << "reconstructed_histograms=h_recon_0..14,h_recon_veto_0..14,"
               "h_recon_vetoed_0..14;TH1I;1000;0;200_MeV\n"
            << "veto=strict_100_lt_each_TDC_lt_4000;count_all_three_faces\n";
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
    const cshine_gamma::ReconstructedEventSummary summary =
        cshine_gamma::BuildReconstructedEventTree(
            cshine_gamma::HistoricalReconstructedEventDefinition(),
            options.inputs, options.output, report, options.overwrite);
    std::cout << "output=" << options.output << '\n'
              << "report=" << report << '\n'
              << "input_files=" << summary.resolved_input_files.size() << '\n'
              << "input_entries=" << summary.input_entries << '\n'
              << "output_entries=" << summary.output_entries << '\n'
              << "valid_reconstructions="
              << summary.valid_reconstruction_count << '\n'
              << "main_spectrum_candidates="
              << summary.main_spectrum_candidate_count << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "build_reconstructed_event_tree: " << error.what() << '\n';
    return 1;
  }
}
