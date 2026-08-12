#include "energy_calibration.h"

#include <TROOT.h>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

struct Options {
  std::string source_spectra_file;
  std::string gain_relation_file;
  std::string output_root_file;
  std::string report_file;
  std::string cobalt_canvas_pdf_file;
  std::string cobalt_canvas_png_file;
  std::string thorium_canvas_pdf_file;
  std::string thorium_canvas_png_file;
  std::string calibration_canvas_pdf_file;
  std::string calibration_canvas_png_file;
  bool overwrite;
  bool print_config;

  Options() : overwrite(false), print_config(false) {}
};

void PrintUsage(const char* program) {
  std::cout
      << "Usage:\n  " << program
      << " --source-spectra FILE --gain-relation FILE --output FILE"
         " [--report FILE]\n"
      << "      [--co-pdf FILE] [--co-png FILE] [--th-pdf FILE]"
         " [--th-png FILE]\n"
      << "      [--calibration-pdf FILE] [--calibration-png FILE]"
         " [--overwrite]\n"
      << "  " << program << " --print-config\n";
}

Options ParseOptions(int argc, char** argv) {
  Options options;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--source-spectra" || argument == "--gain-relation" ||
        argument == "--output" || argument == "--report" ||
        argument == "--co-pdf" || argument == "--co-png" ||
        argument == "--th-pdf" || argument == "--th-png" ||
        argument == "--calibration-pdf" ||
        argument == "--calibration-png") {
      if (index + 1 >= argc) {
        throw std::invalid_argument("missing value after " + argument);
      }
      const std::string value = argv[++index];
      if (argument == "--source-spectra") {
        options.source_spectra_file = value;
      } else if (argument == "--gain-relation") {
        options.gain_relation_file = value;
      } else if (argument == "--output") {
        options.output_root_file = value;
      } else if (argument == "--report") {
        options.report_file = value;
      } else if (argument == "--co-pdf") {
        options.cobalt_canvas_pdf_file = value;
      } else if (argument == "--co-png") {
        options.cobalt_canvas_png_file = value;
      } else if (argument == "--th-pdf") {
        options.thorium_canvas_pdf_file = value;
      } else if (argument == "--th-png") {
        options.thorium_canvas_png_file = value;
      } else if (argument == "--calibration-pdf") {
        options.calibration_canvas_pdf_file = value;
      } else {
        options.calibration_canvas_png_file = value;
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
    const cshine_gamma::EnergyCalibrationDefinition& definition) {
  std::cout << "name=" << definition.name << '\n'
            << "channels=" << definition.channel_count << '\n'
            << "source_histogram_prefix="
            << definition.source_histogram_prefix << '\n'
            << "gain_relation_object="
            << definition.gain_relation_object_name << '\n'
            << "calibration_object=" << definition.calibration_object_name
            << ':' << definition.calibration_object_title << '\n'
            << "reference_energies_mev="
            << definition.reference_energies_mev[0] << ','
            << definition.reference_energies_mev[1] << ','
            << definition.reference_energies_mev[2] << '\n'
            << "point_x_error_semantics=gaussian_peak_width_sigma\n"
            << "cobalt_model=quadratic_background_plus_two_gaussians\n"
            << "thorium_model=gaussian\n"
            << "calibration_model=pol1\n"
            << "canvas_layout=" << definition.canvas_columns << 'x'
            << definition.canvas_rows << '\n';
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const Options options = ParseOptions(argc, argv);
    const cshine_gamma::EnergyCalibrationDefinition definition =
        cshine_gamma::Central0308EnergyCalibrationDefinition();
    if (options.print_config) {
      PrintDefinition(definition);
      return 0;
    }
    if (options.source_spectra_file.empty() ||
        options.gain_relation_file.empty() ||
        options.output_root_file.empty()) {
      PrintUsage(argv[0]);
      return 2;
    }
    const std::string report_file =
        options.report_file.empty() ? options.output_root_file + ".run.tsv"
                                    : options.report_file;
    gROOT->SetBatch(kTRUE);
    const cshine_gamma::EnergyCalibrationSummary summary =
        cshine_gamma::FitEnergyCalibration(
            definition,
            options.source_spectra_file,
            options.gain_relation_file,
            options.output_root_file,
            report_file,
            options.cobalt_canvas_pdf_file,
            options.cobalt_canvas_png_file,
            options.thorium_canvas_pdf_file,
            options.thorium_canvas_png_file,
            options.calibration_canvas_pdf_file,
            options.calibration_canvas_png_file,
            options.overwrite);
    std::cout << "output=" << options.output_root_file << '\n'
              << "report=" << report_file << '\n'
              << "source_spectra=" << options.source_spectra_file << '\n'
              << "gain_relation=" << options.gain_relation_file << '\n'
              << "channels=" << summary.channels.size() << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "fit_energy_calibration: " << error.what() << '\n';
    return 1;
  }
}
