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

TH1* require_histogram(TFile& file, const char* name) {
  TH1* histogram = 0;
  file.GetObject(name, histogram);
  if (!histogram || histogram->GetDimension() != 1) {
    throw std::runtime_error(TString::Format(
        "missing one-dimensional histogram %s in %s", name, file.GetName()).Data());
  }
  return histogram;
}

TH1D* copy_and_rebin(const TH1& source, const char* name) {
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
  TH1D* rebinned = static_cast<TH1D*>(copy->Rebin(5, TString(name) + "_rebin5"));
  rebinned->SetDirectory(0);
  delete copy;
  return rebinned;
}

void style_axis(TH1& histogram) {
  histogram.SetStats(0);
  histogram.GetXaxis()->SetTitle("E_{tot} (MeV)");
  histogram.GetYaxis()->SetTitle("Counts");
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

void draw_slow_coincidence_background_subtraction(
    const char* analysis_root,
    const char* output_directory,
    bool force = false) {
  const TString input_directory =
      TString::Format("%s/DataPreprocessing/step4-convert.0308", analysis_root);
  const TString beam_on_path = input_directory + "/all_recon.root";
  const TString beam_off_path = input_directory + "/all_recon_BKG.root";
  const TString reference_path = input_directory + "/spectrum_110.root";
  const TString output_subdirectory =
      TString(output_directory) + "/slow_coincidence_background_subtraction";
  gSystem->mkdir(output_subdirectory, true);
  const TString output_pdf = output_subdirectory +
      "/cshine_gamma_slow_coincidence_background_subtraction_root_reference.pdf";
  const TString output_png = output_subdirectory +
      "/cshine_gamma_slow_coincidence_background_subtraction_root_reference.png";
  if (!force && (!gSystem->AccessPathName(output_pdf) ||
                 !gSystem->AccessPathName(output_png))) {
    throw std::runtime_error("ROOT-reference output exists; pass force=true to replace");
  }

  TFile beam_on_file(beam_on_path, "READ");
  TFile beam_off_file(beam_off_path, "READ");
  TFile reference_file(reference_path, "READ");
  if (beam_on_file.IsZombie() || beam_off_file.IsZombie() ||
      reference_file.IsZombie()) {
    throw std::runtime_error("cannot open one or more slow-coincidence ROOT inputs");
  }

  TH1D* beam_on = copy_and_rebin(
      *require_histogram(beam_on_file, "h_total_E_M1"), "beam_on");
  TH1D* beam_off = copy_and_rebin(
      *require_histogram(beam_off_file, "h_total_E_M1"), "beam_off");
  TH1* reference = require_histogram(reference_file, "histDiff");

  const int first_bin = beam_on->FindBin(110.0);
  const int last_bin = beam_on->GetNbinsX();
  const double on_counts = beam_on->Integral(first_bin, last_bin);
  const double off_counts = beam_off->Integral(first_bin, last_bin);
  if (on_counts <= 0.0 || off_counts <= 0.0) {
    throw std::runtime_error("normalization integral is not positive");
  }
  const double background_scale = on_counts / off_counts;
  beam_off->Scale(background_scale);
  TH1D* difference = static_cast<TH1D*>(beam_on->Clone("histDiff_candidate"));
  difference->SetDirectory(0);
  difference->Add(beam_off, -1.0);

  if (difference->GetNbinsX() != reference->GetNbinsX()) {
    throw std::runtime_error("candidate and spectrum_110.root bin counts differ");
  }
  double maximum_content_difference = 0.0;
  double maximum_error_difference = 0.0;
  int mismatched_content_bins = 0;
  int mismatched_error_bins = 0;
  for (int bin = 0; bin <= difference->GetNbinsX() + 1; ++bin) {
    const double content_difference =
        std::abs(difference->GetBinContent(bin) - reference->GetBinContent(bin));
    const double error_difference =
        std::abs(difference->GetBinError(bin) - reference->GetBinError(bin));
    maximum_content_difference =
        std::max(maximum_content_difference, content_difference);
    maximum_error_difference = std::max(maximum_error_difference, error_difference);
    if (content_difference > 1e-9) ++mismatched_content_bins;
    if (error_difference > 1e-9) ++mismatched_error_bins;
  }
  if (mismatched_content_bins != 0 || mismatched_error_bins != 0) {
    throw std::runtime_error("candidate does not match spectrum_110.root:histDiff");
  }

  gStyle->SetOptStat(0);
  TCanvas canvas("slow_coincidence", "slow coincidence", 1500, 610);
  canvas.Divide(2, 1, 0.02, 0.0);

  canvas.cd(1);
  gPad->SetLogy();
  gPad->SetLeftMargin(0.14);
  gPad->SetRightMargin(0.04);
  gPad->SetBottomMargin(0.15);
  gPad->SetTopMargin(0.04);
  style_axis(*beam_on);
  beam_on->GetXaxis()->SetRangeUser(0.0, 200.0);
  beam_on->GetYaxis()->SetRangeUser(5e-1, 1e8);
  beam_on->SetLineColor(kBlack);
  beam_on->SetLineWidth(2);
  beam_off->SetLineColor(kRed + 1);
  beam_off->SetLineWidth(2);
  beam_on->Draw("HIST");
  beam_off->Draw("HIST SAME");
  TLegend legend(0.38, 0.72, 0.78, 0.90);
  legend.SetBorderSize(0);
  legend.SetFillStyle(0);
  legend.AddEntry(beam_on, "Beam on", "l");
  legend.AddEntry(beam_off, "Scaled beam off", "l");
  legend.Draw();
  TLatex label_a;
  label_a.SetNDC();
  label_a.SetTextFont(62);
  label_a.SetTextSize(0.055);
  label_a.DrawLatex(0.17, 0.90, "(a)");

  canvas.cd(2);
  gPad->SetLogy();
  gPad->SetLeftMargin(0.14);
  gPad->SetRightMargin(0.04);
  gPad->SetBottomMargin(0.15);
  gPad->SetTopMargin(0.04);
  style_axis(*difference);
  difference->GetXaxis()->SetRangeUser(0.0, 100.0);
  difference->GetYaxis()->SetRangeUser(1e-1, 1e8);
  difference->SetLineColor(kRed + 1);
  difference->SetMarkerColor(kRed + 1);
  difference->SetMarkerStyle(1);
  difference->Draw("E");
  TLatex label_b;
  label_b.SetNDC();
  label_b.SetTextFont(62);
  label_b.SetTextSize(0.055);
  label_b.DrawLatex(0.17, 0.90, "(b)");

  canvas.SaveAs(output_pdf);
  canvas.SaveAs(output_png);
  std::cout << "beam_on_normalization_counts\t" << on_counts << "\n"
            << "beam_off_normalization_counts\t" << off_counts << "\n"
            << "beam_off_scale\t" << background_scale << "\n"
            << "mismatched_content_bins\t" << mismatched_content_bins << "\n"
            << "mismatched_error_bins\t" << mismatched_error_bins << "\n"
            << "maximum_absolute_content_difference\t"
            << maximum_content_difference << "\n"
            << "maximum_absolute_error_difference\t"
            << maximum_error_difference << std::endl;

  delete difference;
  delete beam_off;
  delete beam_on;
}
