#include "observed_spectrum.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

struct Options {
  std::string signal;
  std::string background;
  std::string output;
  std::string report;
  std::string mode;
  bool overwrite = false;
  bool print_config = false;
};

void PrintUsage(const char* program) {
  std::cout << "Usage:\n  " << program
            << " --signal FILE --background FILE --output FILE"
               " --mode slow|fast [--report FILE] [--overwrite]\n  "
            << program << " --print-config\n";
}

Options ParseOptions(int argc, char** argv) {
  Options options;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--signal" || argument == "--background" ||
        argument == "--output" || argument == "--report" ||
        argument == "--mode") {
      if (index + 1 >= argc) {
        throw std::invalid_argument("missing value after " + argument);
      }
      const std::string value = argv[++index];
      if (argument == "--signal") options.signal = value;
      else if (argument == "--background") options.background = value;
      else if (argument == "--output") options.output = value;
      else if (argument == "--report") options.report = value;
      else options.mode = value;
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
  const cshine_gamma::ObservedSpectrumDefinition definition =
      cshine_gamma::HistoricalObservedSpectrumDefinition();
  std::cout << "name=" << definition.name << '\n'
            << "input_histogram=" << definition.input_histogram_name << '\n'
            << "input_binning=" << definition.input_bins << ":0:200_MeV\n"
            << "rebin_factor=" << definition.rebin_factor << '\n'
            << "slow_normalization_MeV=110:200\n"
            << "fast_background_scale=1\n"
            << "output_histogram=" << definition.output_histogram_name << '\n';
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const Options options = ParseOptions(argc, argv);
    if (options.print_config) {
      PrintDefinition();
      return 0;
    }
    if (options.signal.empty() || options.background.empty() ||
        options.output.empty() || options.mode.empty()) {
      PrintUsage(argv[0]);
      return 2;
    }
    cshine_gamma::BackgroundSubtractionMode mode;
    if (options.mode == "slow") {
      mode = cshine_gamma::BackgroundSubtractionMode::kSlowBeamOff;
    } else if (options.mode == "fast") {
      mode = cshine_gamma::BackgroundSubtractionMode::kFastRandomWindow;
    } else {
      throw std::invalid_argument("mode must be slow or fast");
    }
    const std::string report =
        options.report.empty() ? options.output + ".run.tsv" : options.report;
    const cshine_gamma::ObservedSpectrumSummary summary =
        cshine_gamma::BuildObservedSpectrum(
            cshine_gamma::HistoricalObservedSpectrumDefinition(),
            options.signal, options.background, options.output, report, mode,
            options.overwrite);
    std::cout << "mode=" << summary.mode << '\n'
              << "background_scale=" << summary.background_scale << '\n'
              << "output=" << options.output << '\n'
              << "report=" << report << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "build_observed_spectrum: " << error.what() << '\n';
    return 1;
  }
}
