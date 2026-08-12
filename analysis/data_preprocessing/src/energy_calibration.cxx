// Provenance: DP-S200 and DP-S201,
// DataPreprocessing/step2-fit/20240308_ThnatCo60_NoBkg.C and rootlogon.C.

#include "energy_calibration.h"

#include "output_path_support.h"
#include "t_2d_fit.h"
#include "t_gamma_cali.h"

#include <TCanvas.h>
#include <TFile.h>
#include <TF1.h>
#include <TFitResult.h>
#include <TFitResultPtr.h>
#include <TH1.h>
#include <TH2I.h>
#include <TObject.h>
#include <TSystem.h>

#include <cmath>
#include <fstream>
#include <iomanip>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace cshine_gamma {
namespace {

const char* kCobaltFitFormula =
    "[a0]+[a1]*x+[a2]*x*x"
    "+[c0]*exp(-0.5*((x-[m0])/[s0])*((x-[m0])/[s0]))"
    "+[c1]*exp(-0.5*((x-[m1])/[s1])*((x-[m1])/[s1]))";

bool PathExists(const std::string& path) {
  return !gSystem->AccessPathName(path.c_str(), kFileExists);
}

void RequireNewOutputPath(const std::string& path, bool overwrite) {
  if (path.empty()) {
    throw std::invalid_argument("output path must not be empty");
  }
  if (!overwrite && PathExists(path)) {
    throw std::runtime_error("output already exists: " + path);
  }
}

void RequireReadableInput(const std::string& path, const char* role) {
  if (path.empty() || gSystem->AccessPathName(path.c_str(), kReadPermission)) {
    throw std::runtime_error(std::string("missing or unreadable ") + role +
                             " file: " + path);
  }
}

void RequireDistinctPaths(const std::vector<std::string>& paths) {
  std::set<std::string> nonempty;
  for (std::vector<std::string>::const_iterator path = paths.begin();
       path != paths.end(); ++path) {
    if (path->empty()) {
      continue;
    }
    if (!nonempty.insert(*path).second) {
      throw std::invalid_argument("input and output paths must be distinct: " +
                                  *path);
    }
  }
}

void ConfigureCobaltFitFunction(TF1& function) {
  function.SetParLimits(0, 0.0, 10.0);
  function.SetParLimits(1, -10.0, 10.0);
  function.SetParLimits(2, -10.0, 10.0);
  function.SetParLimits(3, 0.0, 10.0);
  function.SetParLimits(4, 0.0, 10.0);
  function.SetParLimits(5, 600.0, 800.0);
  function.SetParLimits(6, 700.0, 900.0);
  function.SetParLimits(7, 5.0, 30.0);
  function.SetParLimits(8, 5.0, 30.0);
}

void RequireFiniteChannelSummary(
    const EnergyCalibrationChannelSummary& summary) {
  const double values[] = {
      summary.cobalt_peak_1_channel,
      summary.cobalt_peak_1_width,
      summary.cobalt_peak_2_channel,
      summary.cobalt_peak_2_width,
      summary.thorium_peak_channel,
      summary.thorium_peak_width,
      summary.intercept_mev,
      summary.intercept_error_mev,
      summary.slope_mev_per_channel,
      summary.slope_error_mev_per_channel,
      summary.calibration_chi_square};
  for (std::size_t index = 0; index < sizeof(values) / sizeof(values[0]);
       ++index) {
    if (!std::isfinite(values[index])) {
      throw std::runtime_error("non-finite energy-calibration result for channel " +
                               std::to_string(summary.channel));
    }
  }
}

}  // namespace

EnergyCalibrationDefinition Central0308EnergyCalibrationDefinition() {
  EnergyCalibrationDefinition definition;
  definition.name = "central_0308";
  definition.channel_count = 15;
  definition.source_histogram_prefix = "h_nobkg_XE_";
  definition.gain_relation_object_name = "f_data";
  definition.calibration_object_name = "cali_20240308";
  definition.calibration_object_title = "20240308";
  definition.reference_energies_mev = {{1.173, 1.332, 2.614}};
  definition.cobalt_fit_windows = {{{{430, 470, 480, 540}},
                                     {{390, 430, 450, 500}},
                                     {{420, 480, 480, 530}},
                                     {{400, 450, 450, 550}},
                                     {{415, 470, 470, 520}},
                                     {{360, 440, 440, 490}},
                                     {{390, 450, 450, 500}},
                                     {{360, 430, 430, 480}},
                                     {{410, 470, 470, 520}},
                                     {{380, 440, 440, 490}},
                                     {{390, 440, 440, 500}},
                                     {{380, 440, 440, 500}},
                                     {{420, 490, 490, 550}},
                                     {{380, 430, 430, 490}},
                                     {{400, 460, 460, 520}}}};
  definition.thorium_fit_windows = {{{{900, 1040}},
                                      {{870, 1000}},
                                      {{900, 1040}},
                                      {{950, 1000}},
                                      {{880, 970}},
                                      {{820, 970}},
                                      {{820, 950}},
                                      {{850, 940}},
                                      {{890, 1000}},
                                      {{850, 950}},
                                      {{870, 960}},
                                      {{880, 970}},
                                      {{930, 1020}},
                                      {{840, 940}},
                                      {{880, 980}}}};
  definition.fit_function_min = 0.0;
  definition.fit_function_max = 4096.0;
  definition.cobalt_canvas_name = "c_1d_fits_co";
  definition.cobalt_canvas_title = "Co-60 Fits";
  definition.thorium_canvas_name = "c_1d_fits_th";
  definition.thorium_canvas_title = "Th-nat Fits";
  definition.calibration_canvas_name = "c_cali";
  definition.calibration_canvas_title = "Calibrated Source";
  definition.fit_canvas_width = 1920;
  definition.fit_canvas_height = 1080;
  definition.calibration_canvas_width = 1920;
  definition.calibration_canvas_height = 1500;
  definition.canvas_columns = 4;
  definition.canvas_rows = 4;
  return definition;
}

void ValidateEnergyCalibrationDefinition(
    const EnergyCalibrationDefinition& definition) {
  if (definition.name.empty() || definition.source_histogram_prefix.empty() ||
      definition.gain_relation_object_name.empty() ||
      definition.calibration_object_name.empty()) {
    throw std::invalid_argument("calibration names must not be empty");
  }
  if (definition.channel_count != 15) {
    throw std::invalid_argument(
        "the historical energy calibration requires exactly 15 channels");
  }
  if (!(definition.fit_function_min < definition.fit_function_max) ||
      definition.canvas_columns <= 0 || definition.canvas_rows <= 0) {
    throw std::invalid_argument("invalid fit-function or canvas definition");
  }
  if (definition.reference_energies_mev[0] != 1.173 ||
      definition.reference_energies_mev[1] != 1.332 ||
      definition.reference_energies_mev[2] != 2.614) {
    throw std::invalid_argument(
        "the historical calibration energies must be 1.173, 1.332, and "
        "2.614 MeV");
  }
  for (unsigned int channel = 0; channel < definition.channel_count;
       ++channel) {
    const std::array<double, 4>& co = definition.cobalt_fit_windows[channel];
    const std::array<double, 2>& th = definition.thorium_fit_windows[channel];
    if (!(co[0] < co[1] && co[1] <= co[2] && co[2] < co[3]) ||
        !(th[0] < th[1])) {
      throw std::invalid_argument("invalid peak-fit window for channel " +
                                  std::to_string(channel));
    }
  }
}

std::string CalibrationSourceHistogramName(unsigned int channel) {
  std::ostringstream name;
  name << "h_nobkg_XE_" << std::setfill('0') << std::setw(2) << channel;
  return name.str();
}

int HistoricalCalibrationCanvasPad(unsigned int channel) {
  if (channel >= 15) {
    throw std::out_of_range("energy-calibration channel is outside 0--14");
  }
  return channel == 12 ? 1 : 16 - static_cast<int>(channel);
}

EnergyCalibrationSummary FitEnergyCalibration(
    const EnergyCalibrationDefinition& definition,
    const std::string& source_spectra_file,
    const std::string& gain_relation_file,
    const std::string& output_root_file,
    const std::string& report_file,
    const std::string& cobalt_canvas_pdf_file,
    const std::string& cobalt_canvas_png_file,
    const std::string& thorium_canvas_pdf_file,
    const std::string& thorium_canvas_png_file,
    const std::string& calibration_canvas_pdf_file,
    const std::string& calibration_canvas_png_file,
    bool overwrite) {
  ValidateEnergyCalibrationDefinition(definition);
  RequireDistinctPaths({source_spectra_file,
                        gain_relation_file,
                        output_root_file,
                        report_file,
                        cobalt_canvas_pdf_file,
                        cobalt_canvas_png_file,
                        thorium_canvas_pdf_file,
                        thorium_canvas_png_file,
                        calibration_canvas_pdf_file,
                        calibration_canvas_png_file});
  RequireReadableInput(source_spectra_file, "source-spectrum");
  RequireReadableInput(gain_relation_file, "gain-relation");
  RequireNewOutputPath(output_root_file, overwrite);
  if (!report_file.empty()) {
    RequireNewOutputPath(report_file, overwrite);
  }
  const std::vector<std::string> optional_outputs = {
      cobalt_canvas_pdf_file,
      cobalt_canvas_png_file,
      thorium_canvas_pdf_file,
      thorium_canvas_png_file,
      calibration_canvas_pdf_file,
      calibration_canvas_png_file};
  for (std::vector<std::string>::const_iterator path = optional_outputs.begin();
       path != optional_outputs.end(); ++path) {
    if (!path->empty()) {
      RequireNewOutputPath(*path, overwrite);
    }
  }

  // The historical macro opened the same source-spectrum ROOT file twice so
  // the Co-60 and Th-nat canvases owned independent histogram instances and
  // retained different displayed x-axis ranges. Keep that behavior here.
  std::unique_ptr<TFile> cobalt_source(
      TFile::Open(source_spectra_file.c_str(), "READ"));
  std::unique_ptr<TFile> thorium_source(
      TFile::Open(source_spectra_file.c_str(), "READ"));
  if (!cobalt_source || cobalt_source->IsZombie() || !thorium_source ||
      thorium_source->IsZombie()) {
    throw std::runtime_error("cannot open source-spectrum file: " +
                             source_spectra_file);
  }
  std::array<TH1*, 15> cobalt_histograms = {{nullptr}};
  std::array<TH1*, 15> thorium_histograms = {{nullptr}};
  for (unsigned int channel = 0; channel < definition.channel_count;
       ++channel) {
    const std::string histogram_name =
        CalibrationSourceHistogramName(channel);
    cobalt_source->GetObject(histogram_name.c_str(), cobalt_histograms[channel]);
    thorium_source->GetObject(histogram_name.c_str(),
                              thorium_histograms[channel]);
    if (cobalt_histograms[channel] == nullptr ||
        thorium_histograms[channel] == nullptr) {
      throw std::runtime_error("missing source histogram: " +
                               histogram_name);
    }
  }

  std::unique_ptr<TFile> gain(TFile::Open(gain_relation_file.c_str(), "READ"));
  if (!gain || gain->IsZombie()) {
    throw std::runtime_error("cannot open gain-relation file: " +
                             gain_relation_file);
  }
  t_2d_fit* gain_relation = nullptr;
  gain->GetObject(definition.gain_relation_object_name.c_str(), gain_relation);
  if (gain_relation == nullptr) {
    throw std::runtime_error("missing t_2d_fit object: " +
                             definition.gain_relation_object_name);
  }

  detail::EnsureOutputParentDirectories({output_root_file,
                                         report_file,
                                         cobalt_canvas_pdf_file,
                                         cobalt_canvas_png_file,
                                         thorium_canvas_pdf_file,
                                         thorium_canvas_png_file,
                                         calibration_canvas_pdf_file,
                                         calibration_canvas_png_file});

  TFile output(output_root_file.c_str(), overwrite ? "RECREATE" : "CREATE");
  if (output.IsZombie()) {
    throw std::runtime_error("cannot create ROOT output: " + output_root_file);
  }

  t_gamma_cali calibration(definition.calibration_object_name.c_str(),
                           definition.calibration_object_title.c_str());
  calibration.Set2D(gain_relation);

  std::array<t_gamma_cali::fit_t, 15> calibration_fits;
  std::vector<std::unique_ptr<TH2I> > calibration_frames;
  calibration_frames.reserve(definition.channel_count);

  TCanvas cobalt_canvas(definition.cobalt_canvas_name.c_str(),
                        definition.cobalt_canvas_title.c_str(),
                        definition.fit_canvas_width,
                        definition.fit_canvas_height);
  cobalt_canvas.Divide(definition.canvas_columns, definition.canvas_rows);
  TCanvas thorium_canvas(definition.thorium_canvas_name.c_str(),
                         definition.thorium_canvas_title.c_str(),
                         definition.fit_canvas_width,
                         definition.fit_canvas_height);
  thorium_canvas.Divide(definition.canvas_columns, definition.canvas_rows);
  const std::string calibration_canvas_title =
      definition.calibration_canvas_title + ": " + source_spectra_file + " " +
      source_spectra_file + ", 2D: " + gain_relation_file;
  TCanvas calibration_canvas(definition.calibration_canvas_name.c_str(),
                             calibration_canvas_title.c_str(),
                             definition.calibration_canvas_width,
                             definition.calibration_canvas_height);
  calibration_canvas.Divide(definition.canvas_columns, definition.canvas_rows);

  TF1 cobalt_fit("gaus2",
                 kCobaltFitFormula,
                 definition.fit_function_min,
                 definition.fit_function_max);
  ConfigureCobaltFitFunction(cobalt_fit);

  EnergyCalibrationSummary summary;
  summary.source_spectra_file = source_spectra_file;
  summary.gain_relation_file = gain_relation_file;
  summary.channels.reserve(definition.channel_count);

  for (unsigned int channel = 0; channel < definition.channel_count;
       ++channel) {
    TH1* cobalt_histogram = cobalt_histograms[channel];
    TH1* thorium_histogram = thorium_histograms[channel];
    const int canvas_pad = HistoricalCalibrationCanvasPad(channel);
    const std::array<double, 4>& co_window =
        definition.cobalt_fit_windows[channel];
    const std::array<double, 2>& th_window =
        definition.thorium_fit_windows[channel];

    cobalt_canvas.cd(canvas_pad);
    cobalt_histogram->GetXaxis()->SetRangeUser(300.0, 700.0);
    cobalt_fit.SetParLimits(5, co_window[0], co_window[1]);
    cobalt_fit.SetParLimits(6, co_window[2], co_window[3]);
    const TFitResultPtr cobalt_result =
        cobalt_histogram->Fit(&cobalt_fit, "S", "", co_window[0], co_window[3]);
    cobalt_histogram->Draw();

    thorium_canvas.cd(canvas_pad);
    thorium_histogram->GetXaxis()->SetRangeUser(700.0, 1200.0);
    const TFitResultPtr thorium_result =
        thorium_histogram->Fit("gaus", "S", "", th_window[0], th_window[1]);
    thorium_histogram->Draw();
    if (cobalt_result.Get() == nullptr || thorium_result.Get() == nullptr) {
      throw std::runtime_error("ROOT peak fit did not return a result for channel " +
                               std::to_string(channel));
    }

    const double point_0 = cobalt_fit.GetParameter(5);
    const double point_0_width = cobalt_fit.GetParameter(7);
    const double point_1 = cobalt_fit.GetParameter(6);
    const double point_1_width = cobalt_fit.GetParameter(8);
    const double point_2 = thorium_result->Parameter(1);
    const double point_2_width = thorium_result->Parameter(2);

    calibration_fits[channel] = calibration.SetCaliPoint(channel,
                                                          point_0,
                                                          point_0_width,
                                                          point_1,
                                                          point_1_width,
                                                          point_2,
                                                          point_2_width);
    output.cd();
    output.WriteTObject(calibration_fits[channel].first.get());
    output.WriteTObject(calibration_fits[channel].second.get());

    EnergyCalibrationChannelSummary channel_summary;
    channel_summary.channel = channel;
    channel_summary.canvas_pad = canvas_pad;
    channel_summary.cobalt_fit_status = static_cast<int>(cobalt_result);
    channel_summary.thorium_fit_status = static_cast<int>(thorium_result);
    channel_summary.cobalt_peak_1_channel = point_0;
    channel_summary.cobalt_peak_1_width = point_0_width;
    channel_summary.cobalt_peak_2_channel = point_1;
    channel_summary.cobalt_peak_2_width = point_1_width;
    channel_summary.thorium_peak_channel = point_2;
    channel_summary.thorium_peak_width = point_2_width;
    TF1* calibration_fit = calibration_fits[channel].second.get();
    channel_summary.intercept_mev = calibration_fit->GetParameter(0);
    channel_summary.intercept_error_mev = calibration_fit->GetParError(0);
    channel_summary.slope_mev_per_channel = calibration_fit->GetParameter(1);
    channel_summary.slope_error_mev_per_channel =
        calibration_fit->GetParError(1);
    channel_summary.calibration_chi_square = calibration_fit->GetChisquare();
    channel_summary.calibration_degrees_of_freedom = calibration_fit->GetNDF();
    RequireFiniteChannelSummary(channel_summary);
    summary.channels.push_back(channel_summary);
  }

  for (unsigned int channel = 0; channel < definition.channel_count;
       ++channel) {
    calibration_canvas.cd(HistoricalCalibrationCanvasPad(channel));
    TGraphErrors* graph = calibration_fits[channel].first.get();
    graph->SetMarkerStyle(1);
    std::unique_ptr<TH2I> frame(
        new TH2I((std::string("h2_") + std::to_string(channel)).c_str(),
                 graph->GetTitle(),
                 1,
                 300.0,
                 1200.0,
                 1,
                 1.0,
                 2.8));
    frame->SetDirectory(nullptr);
    frame->SetStats(false);
    frame->GetXaxis()->SetTitle("ADC XE CH");
    frame->GetYaxis()->SetTitle("Energy (MeV)");
    frame->Draw();
    graph->Draw("SAME");
    calibration_frames.push_back(std::move(frame));
  }

  if (!cobalt_canvas_pdf_file.empty()) {
    cobalt_canvas.SaveAs(cobalt_canvas_pdf_file.c_str());
  }
  if (!cobalt_canvas_png_file.empty()) {
    cobalt_canvas.SaveAs(cobalt_canvas_png_file.c_str());
  }
  if (!thorium_canvas_pdf_file.empty()) {
    thorium_canvas.SaveAs(thorium_canvas_pdf_file.c_str());
  }
  if (!thorium_canvas_png_file.empty()) {
    thorium_canvas.SaveAs(thorium_canvas_png_file.c_str());
  }
  if (!calibration_canvas_pdf_file.empty()) {
    calibration_canvas.SaveAs(calibration_canvas_pdf_file.c_str());
  }
  if (!calibration_canvas_png_file.empty()) {
    calibration_canvas.SaveAs(calibration_canvas_png_file.c_str());
  }

  output.cd();
  output.WriteTObject(&cobalt_canvas);
  output.WriteTObject(&thorium_canvas);
  output.WriteTObject(&calibration);
  output.WriteTObject(&calibration_canvas);
  output.Close();

  if (!report_file.empty()) {
    WriteEnergyCalibrationReport(report_file,
                                 definition,
                                 summary,
                                 output_root_file,
                                 cobalt_canvas_pdf_file,
                                 cobalt_canvas_png_file,
                                 thorium_canvas_pdf_file,
                                 thorium_canvas_png_file,
                                 calibration_canvas_pdf_file,
                                 calibration_canvas_png_file,
                                 overwrite);
  }
  return summary;
}

void WriteEnergyCalibrationReport(
    const std::string& report_file,
    const EnergyCalibrationDefinition& definition,
    const EnergyCalibrationSummary& summary,
    const std::string& output_root_file,
    const std::string& cobalt_canvas_pdf_file,
    const std::string& cobalt_canvas_png_file,
    const std::string& thorium_canvas_pdf_file,
    const std::string& thorium_canvas_png_file,
    const std::string& calibration_canvas_pdf_file,
    const std::string& calibration_canvas_png_file,
    bool overwrite) {
  RequireNewOutputPath(report_file, overwrite);
  std::ofstream report(report_file.c_str(),
                       overwrite ? std::ios::trunc : std::ios::out);
  if (!report) {
    throw std::runtime_error("cannot create run report: " + report_file);
  }
  report << std::setprecision(17);
  report << "record\tfield_1\tfield_2\tfield_3\tfield_4\tfield_5\tfield_6"
            "\tfield_7\tfield_8\tfield_9\tfield_10\tfield_11\tfield_12"
            "\tfield_13\tfield_14\tfield_15\n";
  report << "config\tname\t" << definition.name << '\n';
  report << "config\treference_energies_mev\t"
         << definition.reference_energies_mev[0] << '\t'
         << definition.reference_energies_mev[1] << '\t'
         << definition.reference_energies_mev[2] << '\n';
  report << "config\tpoint_x_error_semantics\tgaussian_peak_width_sigma\n";
  report << "config\toutput_root\t" << output_root_file << '\n';
  report << "config\toutput_object\t"
         << definition.calibration_object_name << '\t'
         << definition.calibration_object_title << '\n';
  report << "input\tsource_spectra\t" << summary.source_spectra_file << '\n';
  report << "input\tgain_relation\t" << summary.gain_relation_file << '\n';
  report << "output\tcobalt_canvas_pdf\t" << cobalt_canvas_pdf_file << '\n';
  report << "output\tcobalt_canvas_png\t" << cobalt_canvas_png_file << '\n';
  report << "output\tthorium_canvas_pdf\t" << thorium_canvas_pdf_file << '\n';
  report << "output\tthorium_canvas_png\t" << thorium_canvas_png_file << '\n';
  report << "output\tcalibration_canvas_pdf\t"
         << calibration_canvas_pdf_file << '\n';
  report << "output\tcalibration_canvas_png\t"
         << calibration_canvas_png_file << '\n';
  for (std::vector<EnergyCalibrationChannelSummary>::const_iterator channel =
           summary.channels.begin();
       channel != summary.channels.end(); ++channel) {
    report << "channel\t" << channel->channel << '\t' << channel->canvas_pad
           << '\t' << channel->cobalt_fit_status << '\t'
           << channel->thorium_fit_status << '\t'
           << channel->cobalt_peak_1_channel << '\t'
           << channel->cobalt_peak_1_width << '\t'
           << channel->cobalt_peak_2_channel << '\t'
           << channel->cobalt_peak_2_width << '\t'
           << channel->thorium_peak_channel << '\t'
           << channel->thorium_peak_width << '\t'
           << channel->intercept_mev << '\t'
           << channel->intercept_error_mev << '\t'
           << channel->slope_mev_per_channel << '\t'
           << channel->slope_error_mev_per_channel << '\t'
           << channel->calibration_chi_square << '\t'
           << channel->calibration_degrees_of_freedom << '\n';
  }
}

}  // namespace cshine_gamma
