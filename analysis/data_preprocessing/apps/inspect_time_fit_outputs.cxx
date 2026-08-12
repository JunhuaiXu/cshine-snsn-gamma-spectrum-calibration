#include "time_fit_outputs.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Options {
  std::string fits_directory;
  std::string report_path;
  double tolerance;
  bool overwrite;
  bool print_config;

  Options() : tolerance(1.0e-9), overwrite(false), print_config(false) {}
};

void PrintUsage(const char* program) {
  std::cerr << "usage: " << program
            << " --fits-dir DIR --report FILE [--tolerance VALUE] [--force]\n"
               "       "
            << program << " --print-config\n";
}

Options ParseOptions(int argc, char** argv) {
  Options options;
  for (int index = 1; index < argc; ++index) {
    const std::string argument(argv[index]);
    if (argument == "--fits-dir" && index + 1 < argc) {
      options.fits_directory = argv[++index];
    } else if (argument == "--report" && index + 1 < argc) {
      options.report_path = argv[++index];
    } else if (argument == "--tolerance" && index + 1 < argc) {
      options.tolerance = std::strtod(argv[++index], 0);
    } else if (argument == "--force") {
      options.overwrite = true;
    } else if (argument == "--print-config") {
      options.print_config = true;
    } else {
      throw std::invalid_argument("unknown or incomplete option: " + argument);
    }
  }
  return options;
}

void PrintConfiguration() {
  std::cout << "stage\tM5 historical time-fit artifact audit\n"
            << "channels\t15 (CsI00--CsI14)\n"
            << "required_root_key\tCanvas_1 containing a TH2 object\n"
            << "required_text_fields\tChi2, NDf, Edm, NCalls, C0, E0, T0\n"
            << "comparison\tC0, E0, T0 against production time parameters\n"
            << "scope\tread-only audit; no historical refit\n";
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const Options options = ParseOptions(argc, argv);
    if (options.print_config) {
      PrintConfiguration();
      return 0;
    }
    if (options.fits_directory.empty() || options.report_path.empty()) {
      PrintUsage(argv[0]);
      return 2;
    }

    const std::vector<cshine_gamma::TimeFitArtifactSummary> summaries =
        cshine_gamma::InspectTimeFitArtifacts(options.fits_directory,
                                              options.tolerance);
    cshine_gamma::WriteTimeFitArtifactReport(
        options.report_path, summaries, options.overwrite);

    bool all_match = summaries.size() == 15;
    for (std::vector<cshine_gamma::TimeFitArtifactSummary>::const_iterator item =
             summaries.begin();
         item != summaries.end(); ++item) {
      all_match = all_match && item->parameters_match_production;
    }
    if (!all_match) {
      std::cerr << "historical fit parameters do not match production values\n";
      return 1;
    }
    std::cout << "verified 15 historical time-fit ROOT/text pairs; production "
                 "parameters match\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "inspect_time_fit_outputs: " << error.what() << '\n';
    return 1;
  }
}
