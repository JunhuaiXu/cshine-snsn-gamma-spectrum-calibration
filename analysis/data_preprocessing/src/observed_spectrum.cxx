// Direct M11 migration of DP-S604 and the fast-window EnergySpecGen.C.

#include "observed_spectrum.h"

#include "output_path_support.h"

#include <TFile.h>
#include <TH1.h>
#include <TH1D.h>
#include <TSystem.h>

#include <cmath>
#include <fstream>
#include <iomanip>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace cshine_gamma {
namespace {

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

TH1* RequireHistogram(TFile& file,
                      const ObservedSpectrumDefinition& definition) {
  TH1* histogram = nullptr;
  file.GetObject(definition.input_histogram_name.c_str(), histogram);
  if (histogram == nullptr || histogram->GetDimension() != 1) {
    throw std::runtime_error("missing one-dimensional input histogram in " +
                             std::string(file.GetName()));
  }
  if (histogram->GetNbinsX() != definition.input_bins ||
      std::abs(histogram->GetXaxis()->GetXmin() -
               definition.energy_min_mev) > 1.0e-12 ||
      std::abs(histogram->GetXaxis()->GetXmax() -
               definition.energy_max_mev) > 1.0e-12) {
    throw std::runtime_error("unexpected input histogram schema in " +
                             std::string(file.GetName()));
  }
  return histogram;
}

std::unique_ptr<TH1D> CopyAndRebin(
    const TH1& source,
    const std::string& name,
    const ObservedSpectrumDefinition& definition) {
  std::unique_ptr<TH1D> copy(new TH1D(
      name.c_str(), name.c_str(), definition.input_bins,
      definition.energy_min_mev, definition.energy_max_mev));
  copy->SetDirectory(nullptr);
  copy->Add(&source);
  copy->Sumw2();
  copy->Rebin(definition.rebin_factor);
  return copy;
}

const char* ModeName(BackgroundSubtractionMode mode) {
  return mode == BackgroundSubtractionMode::kSlowBeamOff
             ? "slow-beam-off"
             : "fast-random-window";
}

void WriteReport(const ObservedSpectrumDefinition& definition,
                 const ObservedSpectrumSummary& summary,
                 const std::string& signal_root_file,
                 const std::string& background_root_file,
                 const std::string& output_root_file,
                 const std::string& report_file) {
  if (report_file.empty()) {
    return;
  }
  std::ofstream report(report_file.c_str());
  if (!report) {
    throw std::runtime_error("cannot create run report: " + report_file);
  }
  report << std::setprecision(17);
  report << "record\tfield_1\tfield_2\tfield_3\n";
  report << "config\tname\t" << definition.name << "\n";
  report << "config\tmode\t" << summary.mode << "\n";
  report << "config\tinput_histogram\t"
         << definition.input_histogram_name << "\n";
  report << "config\tinput_binning\t" << definition.input_bins << '\t'
         << definition.energy_min_mev << ':' << definition.energy_max_mev
         << "\n";
  report << "config\trebin_factor\t" << definition.rebin_factor << "\n";
  report << "config\tnormalization_interval_mev\t"
         << definition.normalization_min_mev << ':'
         << definition.normalization_max_mev << "\n";
  report << "input\tsignal\t" << signal_root_file << "\n";
  report << "input\tbackground\t" << background_root_file << "\n";
  report << "summary\tsignal_normalization_counts\t"
         << summary.signal_normalization_counts << "\n";
  report << "summary\tbackground_normalization_counts\t"
         << summary.background_normalization_counts << "\n";
  report << "summary\tbackground_scale\t" << summary.background_scale
         << "\n";
  report << "output\troot\t" << output_root_file << '\t'
         << definition.output_histogram_name << "\n";
  report << "output\tbinning\t" << summary.output_bins << '\t'
         << summary.output_energy_min_mev << ':'
         << summary.output_energy_max_mev << "\n";
}

void WriteInterfaceReport(
    const ObservedSpectrumInterfaceDefinition& definition,
    const ObservedSpectrumInterfaceSummary& summary,
    const std::string& input_root_file,
    const std::string& report_file) {
  std::ofstream report(report_file.c_str());
  if (!report) {
    throw std::runtime_error("cannot create interface report: " + report_file);
  }
  report << std::setprecision(17);
  report << "record\tfield\tvalue\tdetail\n";
  report << "input\troot_file\t" << input_root_file << "\n";
  report << "contract\tobject_name\t" << definition.object_name << "\n";
  report << "contract\tobject_class\t" << definition.object_class << "\n";
  report << "contract\tenergy_frame\t" << definition.energy_frame << "\n";
  report << "contract\tvalue_semantics\t" << definition.value_semantics
         << "\n";
  report << "contract\tuncertainty_semantics\t"
         << definition.uncertainty_semantics << "\n";
  report << "contract\tconsumer_rule\tread_as_TH1D_or_TH1_base"
         << "\tdo_not_cast_to_TH1F\n";
  report << "schema\tbins\t" << summary.bins << "\n";
  report << "schema\tenergy_range_mev\t" << summary.energy_min_mev << ':'
         << summary.energy_max_mev << "\n";
  report << "schema\tbin_width_mev\t" << summary.bin_width_mev << "\n";
  report << "summary\tregular_bin_sum\t" << summary.regular_bin_sum << "\n";
  report << "summary\tregular_variance_sum\t"
         << summary.regular_variance_sum << "\n";
  report << "summary\tnegative_regular_bins\t"
         << summary.negative_regular_bins << "\n";
  report << "flow\tunderflow\t" << summary.underflow << '\t'
         << summary.underflow_error << "\n";
  report << "flow\toverflow\t" << summary.overflow << '\t'
         << summary.overflow_error << "\n";
}

}  // namespace

ObservedSpectrumDefinition HistoricalObservedSpectrumDefinition() {
  ObservedSpectrumDefinition definition;
  definition.name = "central_0308_observed_spectrum";
  definition.input_histogram_name = "h_total_E_M1";
  definition.output_histogram_name = "histDiff";
  definition.input_bins = 1000;
  definition.energy_min_mev = 0.0;
  definition.energy_max_mev = 200.0;
  definition.rebin_factor = 5;
  definition.normalization_min_mev = 110.0;
  definition.normalization_max_mev = 200.0;
  return definition;
}

ObservedSpectrumInterfaceDefinition
HistoricalObservedSpectrumInterfaceDefinition() {
  ObservedSpectrumInterfaceDefinition definition;
  definition.object_name = "histDiff";
  definition.object_class = "TH1D";
  definition.bins = 200;
  definition.energy_min_mev = 0.0;
  definition.energy_max_mev = 200.0;
  definition.bin_width_mev = 1.0;
  definition.energy_frame = "laboratory";
  definition.value_semantics = "background-subtracted event counts";
  definition.uncertainty_semantics = "per-bin standard error";
  return definition;
}

void ValidateObservedSpectrumDefinition(
    const ObservedSpectrumDefinition& definition) {
  const ObservedSpectrumDefinition historical =
      HistoricalObservedSpectrumDefinition();
  if (definition.name != historical.name ||
      definition.input_histogram_name != historical.input_histogram_name ||
      definition.output_histogram_name != historical.output_histogram_name ||
      definition.input_bins != historical.input_bins ||
      definition.energy_min_mev != historical.energy_min_mev ||
      definition.energy_max_mev != historical.energy_max_mev ||
      definition.rebin_factor != historical.rebin_factor ||
      definition.normalization_min_mev !=
          historical.normalization_min_mev ||
      definition.normalization_max_mev !=
          historical.normalization_max_mev) {
    throw std::invalid_argument("historical observed-spectrum definition changed");
  }
}

ObservedSpectrumSummary BuildObservedSpectrum(
    const ObservedSpectrumDefinition& definition,
    const std::string& signal_root_file,
    const std::string& background_root_file,
    const std::string& output_root_file,
    const std::string& report_file,
    BackgroundSubtractionMode mode,
    bool overwrite) {
  ValidateObservedSpectrumDefinition(definition);
  if (signal_root_file == background_root_file) {
    throw std::invalid_argument("signal and background inputs must differ");
  }
  if (!report_file.empty() && report_file == output_root_file) {
    throw std::invalid_argument("ROOT output and report paths must differ");
  }
  RequireNewOutputPath(output_root_file, overwrite);
  if (!report_file.empty()) {
    RequireNewOutputPath(report_file, overwrite);
  }

  std::unique_ptr<TFile> signal_file(TFile::Open(signal_root_file.c_str(), "READ"));
  std::unique_ptr<TFile> background_file(
      TFile::Open(background_root_file.c_str(), "READ"));
  if (!signal_file || signal_file->IsZombie() || !background_file ||
      background_file->IsZombie()) {
    throw std::runtime_error("cannot open signal or background ROOT input");
  }
  std::unique_ptr<TH1D> signal(CopyAndRebin(
      *RequireHistogram(*signal_file, definition), "hist_gamma", definition));
  std::unique_ptr<TH1D> background(CopyAndRebin(
      *RequireHistogram(*background_file, definition), "hist_BKG", definition));

  const int first_bin = signal->FindBin(definition.normalization_min_mev);
  const int last_bin = signal->GetNbinsX();
  ObservedSpectrumSummary summary = {};
  summary.mode = ModeName(mode);
  summary.signal_normalization_counts = signal->Integral(first_bin, last_bin);
  summary.background_normalization_counts =
      background->Integral(first_bin, last_bin);
  if (mode == BackgroundSubtractionMode::kSlowBeamOff) {
    if (summary.signal_normalization_counts <= 0.0 ||
        summary.background_normalization_counts <= 0.0) {
      throw std::runtime_error("slow-background normalization integral is not positive");
    }
    summary.background_scale = summary.signal_normalization_counts /
                               summary.background_normalization_counts;
  } else {
    summary.background_scale = 1.0;
  }
  background->Scale(summary.background_scale);

  std::unique_ptr<TH1D> difference(
      dynamic_cast<TH1D*>(signal->Clone(definition.output_histogram_name.c_str())));
  if (!difference) {
    throw std::runtime_error("cannot clone observed-spectrum histogram");
  }
  difference->SetDirectory(nullptr);
  difference->Add(background.get(), -1.0);
  summary.output_bins = difference->GetNbinsX();
  summary.output_energy_min_mev = difference->GetXaxis()->GetXmin();
  summary.output_energy_max_mev = difference->GetXaxis()->GetXmax();

  detail::EnsureOutputParentDirectories(
      std::vector<std::string>{output_root_file, report_file});
  std::unique_ptr<TFile> output(TFile::Open(output_root_file.c_str(), "RECREATE"));
  if (!output || output->IsZombie()) {
    throw std::runtime_error("cannot create observed-spectrum ROOT output");
  }
  output->cd();
  difference->Write();
  output->Close();
  WriteReport(definition, summary, signal_root_file, background_root_file,
              output_root_file, report_file);
  return summary;
}

ObservedSpectrumInterfaceSummary InspectObservedSpectrumInterface(
    const ObservedSpectrumInterfaceDefinition& definition,
    const std::string& input_root_file,
    const std::string& report_file,
    bool overwrite) {
  const ObservedSpectrumInterfaceDefinition historical =
      HistoricalObservedSpectrumInterfaceDefinition();
  if (definition.object_name != historical.object_name ||
      definition.object_class != historical.object_class ||
      definition.bins != historical.bins ||
      definition.energy_min_mev != historical.energy_min_mev ||
      definition.energy_max_mev != historical.energy_max_mev ||
      definition.bin_width_mev != historical.bin_width_mev ||
      definition.energy_frame != historical.energy_frame ||
      definition.value_semantics != historical.value_semantics ||
      definition.uncertainty_semantics != historical.uncertainty_semantics) {
    throw std::invalid_argument("historical observed-spectrum interface changed");
  }
  if (input_root_file.empty() || report_file.empty()) {
    throw std::invalid_argument("input ROOT file and report path are required");
  }
  if (input_root_file == report_file) {
    throw std::invalid_argument("input ROOT file and report path must differ");
  }
  RequireNewOutputPath(report_file, overwrite);

  std::unique_ptr<TFile> input(TFile::Open(input_root_file.c_str(), "READ"));
  if (!input || input->IsZombie()) {
    throw std::runtime_error("cannot open observed-spectrum ROOT input");
  }
  TH1D* histogram = nullptr;
  input->GetObject(definition.object_name.c_str(), histogram);
  if (histogram == nullptr || histogram->GetDimension() != 1) {
    throw std::runtime_error("missing TH1D observed-spectrum object: " +
                             definition.object_name);
  }
  if (std::string(histogram->ClassName()) != definition.object_class) {
    throw std::runtime_error("unexpected observed-spectrum object class");
  }
  if (histogram->GetNbinsX() != definition.bins ||
      std::abs(histogram->GetXaxis()->GetXmin() -
               definition.energy_min_mev) > 1.0e-12 ||
      std::abs(histogram->GetXaxis()->GetXmax() -
               definition.energy_max_mev) > 1.0e-12) {
    throw std::runtime_error("unexpected observed-spectrum axis schema");
  }
  if (histogram->GetSumw2N() == 0) {
    throw std::runtime_error("observed spectrum has no stored per-bin variances");
  }

  ObservedSpectrumInterfaceSummary summary = {};
  summary.object_class = histogram->ClassName();
  summary.bins = histogram->GetNbinsX();
  summary.energy_min_mev = histogram->GetXaxis()->GetXmin();
  summary.energy_max_mev = histogram->GetXaxis()->GetXmax();
  summary.bin_width_mev = histogram->GetXaxis()->GetBinWidth(1);
  if (std::abs(summary.bin_width_mev - definition.bin_width_mev) > 1.0e-12) {
    throw std::runtime_error("unexpected observed-spectrum bin width");
  }
  for (int bin = 1; bin <= histogram->GetNbinsX(); ++bin) {
    const double width = histogram->GetXaxis()->GetBinWidth(bin);
    const double content = histogram->GetBinContent(bin);
    const double error = histogram->GetBinError(bin);
    if (std::abs(width - definition.bin_width_mev) > 1.0e-12) {
      throw std::runtime_error("observed-spectrum energy bins are not uniform");
    }
    if (!std::isfinite(content) || !std::isfinite(error) || error < 0.0) {
      throw std::runtime_error("observed spectrum contains invalid bin values");
    }
    summary.regular_bin_sum += content;
    summary.regular_variance_sum += error * error;
    if (content < 0.0) ++summary.negative_regular_bins;
  }
  summary.underflow = histogram->GetBinContent(0);
  summary.underflow_error = histogram->GetBinError(0);
  summary.overflow = histogram->GetBinContent(histogram->GetNbinsX() + 1);
  summary.overflow_error = histogram->GetBinError(histogram->GetNbinsX() + 1);
  if (!std::isfinite(summary.underflow) ||
      !std::isfinite(summary.underflow_error) ||
      !std::isfinite(summary.overflow) ||
      !std::isfinite(summary.overflow_error)) {
    throw std::runtime_error("observed spectrum contains invalid flow bins");
  }

  detail::EnsureOutputParentDirectories(std::vector<std::string>{report_file});
  WriteInterfaceReport(definition, summary, input_root_file, report_file);
  return summary;
}

}  // namespace cshine_gamma
