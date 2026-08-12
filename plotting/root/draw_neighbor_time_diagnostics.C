// Render the six authoritative M7 neighboring-crystal timing objects.
//
// Numerical producer:
//   analysis/data_preprocessing/apps/build_neighbor_time_diagnostics.cxx
// Historical source:
//   DataPreprocessing/step4-convert.0308.PreRun/draw_GammaTimeDiff.C

#include <TCanvas.h>
#include <TError.h>
#include <TFile.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TLegend.h>
#include <TROOT.h>
#include <TString.h>
#include <TStyle.h>
#include <TSystem.h>

#include <iostream>

namespace {

void ConfigureCanvas(TCanvas &canvas, bool with_color_bar = false) {
  canvas.SetLeftMargin(0.18);
  canvas.SetBottomMargin(0.18);
  canvas.SetRightMargin(with_color_bar ? 0.12 : 0.06);
  canvas.SetTopMargin(0.06);
  canvas.SetTicks(1, 1);
}

void StyleOneDimensional(TH1 &histogram, const char *x_title) {
  histogram.SetStats(false);
  histogram.SetTitle("");
  histogram.GetXaxis()->SetTitle(x_title);
  histogram.GetYaxis()->SetTitle("Counts");
  histogram.GetXaxis()->CenterTitle();
  histogram.GetYaxis()->CenterTitle();
  histogram.GetXaxis()->SetNdivisions(505);
  histogram.GetYaxis()->SetNdivisions(505);
  histogram.GetXaxis()->SetTitleSize(0.07);
  histogram.GetYaxis()->SetTitleSize(0.07);
  histogram.GetXaxis()->SetLabelSize(0.06);
  histogram.GetYaxis()->SetLabelSize(0.06);
  histogram.GetYaxis()->SetTitleOffset(1.25);
  histogram.SetLineWidth(2);
}

void StyleTwoDimensional(TH2 &histogram) {
  histogram.SetStats(false);
  histogram.SetTitle(";T_{5} [ns];T_{6} [ns]");
  histogram.GetXaxis()->CenterTitle();
  histogram.GetYaxis()->CenterTitle();
  histogram.GetXaxis()->SetNdivisions(505);
  histogram.GetYaxis()->SetNdivisions(505);
  histogram.GetXaxis()->SetTitleSize(0.07);
  histogram.GetYaxis()->SetTitleSize(0.07);
  histogram.GetXaxis()->SetLabelSize(0.06);
  histogram.GetYaxis()->SetLabelSize(0.06);
  histogram.GetYaxis()->SetTitleOffset(1.25);
}

}  // namespace

void draw_neighbor_time_diagnostics(
    const char *diagnostics_root,
    const char *output_dir = "results/figures/neighbor_time/root") {
  gROOT->SetBatch(true);
  gStyle->SetPalette(kBird);
  gStyle->SetNumberContours(255);

  TFile input(diagnostics_root, "READ");
  if (input.IsZombie()) {
    Error("draw_neighbor_time_diagnostics", "Cannot open %s", diagnostics_root);
    return;
  }
  TH2F *h2_all = nullptr;
  TH2F *h2_cut = nullptr;
  TH1F *h1 = nullptr;
  TH1F *hh_diff = nullptr;
  TH1F *h3 = nullptr;
  TH1F *h4 = nullptr;
  input.GetObject("h2_all", h2_all);
  input.GetObject("h2_cut", h2_cut);
  input.GetObject("h1", h1);
  input.GetObject("hh_diff", hh_diff);
  input.GetObject("h3", h3);
  input.GetObject("h4", h4);
  if (!h2_all || !h2_cut || !h1 || !hh_diff || !h3 || !h4) {
    Error("draw_neighbor_time_diagnostics",
          "M7 file must contain h2_all, h2_cut, h1, hh_diff, h3, and h4");
    return;
  }
  if (hh_diff->GetMaximum() <= 0.0) {
    Error("draw_neighbor_time_diagnostics", "hh_diff is empty");
    return;
  }
  if (gSystem->mkdir(output_dir, true) != 0 &&
      gSystem->AccessPathName(output_dir)) {
    Error("draw_neighbor_time_diagnostics", "Cannot create %s", output_dir);
    return;
  }

  TH2F all_display(*h2_all);
  TH2F cut_display(*h2_cut);
  StyleTwoDimensional(all_display);
  StyleTwoDimensional(cut_display);
  TCanvas all_canvas("neighbor_all", "Neighbor time without energy cut", 800, 600);
  ConfigureCanvas(all_canvas, true);
  all_canvas.SetLogz();
  all_display.Draw("COLZ");
  all_canvas.SaveAs(TString(output_dir) + "/GammaTime2D_all.pdf");
  TCanvas cut_canvas("neighbor_cut", "Neighbor time with energy cut", 800, 600);
  ConfigureCanvas(cut_canvas, true);
  cut_canvas.SetLogz();
  cut_display.Draw("COLZ");
  cut_canvas.SaveAs(TString(output_dir) + "/GammaTime2D_cut.pdf");

  TH1F difference_cut_only(*hh_diff);
  StyleOneDimensional(difference_cut_only, "T_{5}-T_{6} [ns]");
  difference_cut_only.SetLineColor(kRed + 1);
  TCanvas cut_difference_canvas("difference_cut", "Selected time difference", 800, 600);
  ConfigureCanvas(cut_difference_canvas);
  difference_cut_only.Draw("hist");
  cut_difference_canvas.SaveAs(TString(output_dir) + "/T5_minus_T6_cut.pdf");

  TH1F difference_all(*h1);
  TH1F difference_cut(*hh_diff);
  const double peak_scale = difference_all.GetMaximum() /
                            difference_cut.GetMaximum();
  difference_cut.Scale(peak_scale);
  StyleOneDimensional(difference_all, "T_{5}-T_{6} [ns]");
  StyleOneDimensional(difference_cut, "T_{5}-T_{6} [ns]");
  difference_all.SetLineColor(kBlue + 1);
  difference_cut.SetLineColor(kRed + 1);
  TCanvas difference_canvas("difference", "Neighbor time difference", 800, 600);
  ConfigureCanvas(difference_canvas);
  difference_all.Draw("hist");
  difference_cut.Draw("hist same");
  TLegend legend(0.20, 0.58, 0.51, 0.86);
  legend.SetBorderSize(0);
  legend.SetFillStyle(0);
  legend.AddEntry(&difference_all, "No energy cut", "l");
  legend.AddEntry(&difference_cut, "E_{5}+E_{6} #geq 30 MeV", "l");
  legend.Draw();
  difference_canvas.SaveAs(TString(output_dir) + "/TimeDiff.pdf");

  TH1F time5(*h3);
  TH1F time6(*h4);
  StyleOneDimensional(time5, "T_{5} [ns]");
  StyleOneDimensional(time6, "T_{6} [ns]");
  time5.SetLineColor(kBlue + 1);
  time6.SetLineColor(kBlue + 1);
  TCanvas time5_canvas("time5", "CsI05 time", 800, 600);
  ConfigureCanvas(time5_canvas);
  time5.Draw("hist");
  time5_canvas.SaveAs(TString(output_dir) + "/Time5.pdf");
  TCanvas time6_canvas("time6", "CsI06 time", 800, 600);
  ConfigureCanvas(time6_canvas);
  time6.Draw("hist");
  time6_canvas.SaveAs(TString(output_dir) + "/Time6.pdf");

  std::cout << "historical_peak_scale=" << peak_scale << std::endl;
}
