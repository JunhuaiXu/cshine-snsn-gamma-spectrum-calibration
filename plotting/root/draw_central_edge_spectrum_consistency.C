#include <TCanvas.h>
#include <TFile.h>
#include <TH1.h>
#include <TH1D.h>
#include <TLegend.h>
#include <TLatex.h>
#include <TString.h>
#include <TStyle.h>
#include <TSystem.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

const char* kObjectNames[3] = {
    "h_central_E_M1", "h_side_E_M1", "h_total_E_M1"};
const char* kLabels[3] = {"Central", "Edge", "Total"};
const int kColors[3] = {kBlue + 1, kGreen + 2, kRed + 1};

TH1* require_histogram_central_edge(TFile& file, const char* name) {
  TH1* histogram = 0;
  file.GetObject(name, histogram);
  if (!histogram || histogram->GetDimension() != 1) {
    throw std::runtime_error(TString::Format(
        "missing one-dimensional histogram %s in %s", name, file.GetName()).Data());
  }
  return histogram;
}

TH1D* copy_and_rebin_central_edge(const TH1& source, const char* name) {
  const TAxis* axis = source.GetXaxis();
  if (source.GetNbinsX() != 1000 || std::abs(axis->GetXmin()) > 1e-12 ||
      std::abs(axis->GetXmax() - 200.0) > 1e-12) {
    throw std::runtime_error("source histogram is not 1000 bins over 0--200 MeV");
  }
  TH1D* copy = new TH1D(name, "", source.GetNbinsX(), axis->GetXmin(),
                        axis->GetXmax());
  copy->SetDirectory(0);
  copy->Add(&source);
  copy->Sumw2();
  TH1D* rebinned =
      static_cast<TH1D*>(copy->Rebin(5, TString(name) + "_rebin5"));
  rebinned->SetDirectory(0);
  delete copy;
  return rebinned;
}

TH1D* subtract_scaled_background(const TH1& signal_source,
                                 const TH1& background_source,
                                 const char* name,
                                 double& background_scale) {
  TH1D* signal = copy_and_rebin_central_edge(
      signal_source, TString(name) + "_beam_on");
  TH1D* background = copy_and_rebin_central_edge(
      background_source, TString(name) + "_beam_off");

  // Preserve the historical ROOT FindBin/Integral semantics exactly.  Since
  // 200 MeV is the upper axis edge, the end bin includes the overflow bin.
  const int first_bin = signal->FindBin(110.0);
  const int last_bin = signal->FindBin(200.0);
  const double signal_cosmic = signal->Integral(first_bin, last_bin);
  const double background_cosmic = background->Integral(first_bin, last_bin);
  if (background_cosmic == 0.0) {
    throw std::runtime_error("zero beam-off integral in 110--200 MeV normalization region");
  }
  background_scale = signal_cosmic / background_cosmic;
  background->Scale(background_scale);

  TH1D* difference = static_cast<TH1D*>(signal->Clone(name));
  difference->SetDirectory(0);
  difference->Add(background, -1.0);
  delete background;
  delete signal;
  return difference;
}

TH1D* normalize_to_total(const TH1D& source, double total_integral,
                         const char* name, double& scale) {
  TH1D* normalized = static_cast<TH1D*>(source.Clone(name));
  normalized->SetDirectory(0);
  const int first_bin = normalized->FindBin(35.0);
  const int last_bin = normalized->FindBin(100.0);
  const double integral = normalized->Integral(first_bin, last_bin);
  if (integral == 0.0) {
    throw std::runtime_error("zero subtracted-spectrum integral in normalization region");
  }
  scale = total_integral / integral;
  normalized->Scale(scale);
  return normalized;
}

void style_histogram_central_edge(TH1& histogram, const char* y_title) {
  histogram.SetStats(0);
  histogram.GetXaxis()->SetTitle("E_{tot} (MeV)");
  histogram.GetYaxis()->SetTitle(y_title);
  histogram.GetXaxis()->CenterTitle();
  histogram.GetYaxis()->CenterTitle();
  histogram.GetXaxis()->SetNdivisions(1005);
  histogram.GetYaxis()->SetNdivisions(1005);
  histogram.GetXaxis()->SetTitleSize(0.055);
  histogram.GetYaxis()->SetTitleSize(0.055);
  histogram.GetXaxis()->SetLabelSize(0.045);
  histogram.GetYaxis()->SetLabelSize(0.045);
  histogram.GetYaxis()->SetTitleOffset(1.25);
}

}  // namespace

void draw_central_edge_spectrum_consistency(
    const char* analysis_root,
    const char* output_directory,
    bool force = false) {
  const TString input_directory = TString::Format(
      "%s/DataPreprocessing/step4-convert.0308.PreRun", analysis_root);
  const TString signal_path = input_directory + "/all_recon.root";
  const TString background_path = input_directory + "/all_recon_BKG.root";
  const TString output_subdirectory =
      TString(output_directory) + "/central_edge_spectrum_consistency";
  gSystem->mkdir(output_subdirectory, true);

  const TString output_root = output_subdirectory +
      "/cshine_gamma_central_edge_spectrum_consistency_root_reference.root";
  const TString output_pdf = output_subdirectory +
      "/cshine_gamma_central_edge_spectrum_consistency_root_reference.pdf";
  const TString output_png = output_subdirectory +
      "/cshine_gamma_central_edge_spectrum_consistency_root_reference.png";
  if (!force && (!gSystem->AccessPathName(output_root) ||
                 !gSystem->AccessPathName(output_pdf) ||
                 !gSystem->AccessPathName(output_png))) {
    throw std::runtime_error(
        "ROOT-reference output exists; pass force=true to replace");
  }

  TFile signal_file(signal_path, "READ");
  TFile background_file(background_path, "READ");
  if (signal_file.IsZombie() || background_file.IsZombie()) {
    throw std::runtime_error("cannot open central/edge beam-on or beam-off input");
  }

  TH1D* subtracted[3] = {0, 0, 0};
  TH1D* normalized[3] = {0, 0, 0};
  double background_scales[3] = {0.0, 0.0, 0.0};
  double normalization_scales[3] = {0.0, 0.0, 0.0};

  for (int index = 0; index < 3; ++index) {
    subtracted[index] = subtract_scaled_background(
        *require_histogram_central_edge(signal_file, kObjectNames[index]),
        *require_histogram_central_edge(background_file, kObjectNames[index]),
        TString::Format("%s_subtracted", kObjectNames[index]),
        background_scales[index]);
  }

  const int first_bin = subtracted[2]->FindBin(35.0);
  const int last_bin = subtracted[2]->FindBin(100.0);
  const double total_integral = subtracted[2]->Integral(first_bin, last_bin);
  for (int index = 0; index < 3; ++index) {
    normalized[index] = normalize_to_total(
        *subtracted[index], total_integral,
        TString::Format("%s_normalized_to_total", kObjectNames[index]),
        normalization_scales[index]);
  }

  TFile reference_file(output_root, "RECREATE");
  if (reference_file.IsZombie()) {
    throw std::runtime_error("cannot create ROOT-reference output");
  }
  for (int index = 0; index < 3; ++index) {
    subtracted[index]->Write();
    normalized[index]->Write();
  }
  reference_file.Close();

  gStyle->SetOptStat(0);
  TCanvas canvas("central_edge", "central and edge spectra", 1500, 610);
  canvas.Divide(2, 1, 0.02, 0.0);
  for (int panel = 0; panel < 2; ++panel) {
    canvas.cd(panel + 1);
    gPad->SetLogy();
    gPad->SetLeftMargin(0.14);
    gPad->SetRightMargin(0.04);
    gPad->SetBottomMargin(0.15);
    gPad->SetTopMargin(0.04);
    TH1D** spectra = panel == 0 ? subtracted : normalized;
    style_histogram_central_edge(
        *spectra[0], panel == 0 ? "Counts" : "Counts normalized to total");
    spectra[0]->GetXaxis()->SetRangeUser(0.0, 100.0);
    spectra[0]->SetMinimum(5e-1);
    double maximum = 0.0;
    for (int index = 0; index < 3; ++index) {
      maximum = std::max(maximum, spectra[index]->GetMaximum());
      spectra[index]->SetLineColor(kColors[index]);
      spectra[index]->SetLineWidth(2);
    }
    spectra[0]->SetMaximum(maximum * 3.0);
    spectra[0]->Draw("HIST");
    spectra[1]->Draw("HIST SAME");
    spectra[2]->Draw("HIST SAME");

    TLegend legend(0.62, 0.72, 0.91, 0.91);
    legend.SetBorderSize(0);
    legend.SetFillStyle(0);
    for (int index = 0; index < 3; ++index) {
      legend.AddEntry(spectra[index], kLabels[index], "l");
    }
    legend.Draw();
    TLatex label;
    label.SetNDC();
    label.SetTextFont(62);
    label.SetTextSize(0.055);
    label.DrawLatex(0.17, 0.90, panel == 0 ? "(a)" : "(b)");
  }
  canvas.SaveAs(output_pdf);
  canvas.SaveAs(output_png);

  for (int index = 0; index < 3; ++index) {
    std::cout << kLabels[index] << "_background_scale\t"
              << background_scales[index] << "\n"
              << kLabels[index] << "_shape_scale\t"
              << normalization_scales[index] << "\n";
    delete normalized[index];
    delete subtracted[index];
  }
}
