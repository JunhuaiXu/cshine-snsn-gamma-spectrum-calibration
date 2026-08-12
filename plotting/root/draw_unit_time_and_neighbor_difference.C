// Portable ROOT reference for analysis-note Fig. 6.
//
// Historical source:
//   DataPreprocessing/step4-convert.0308.PreRun/draw_GammaTimeDiff.C
//
// This macro keeps ROOT responsible for the event selection, histogram
// filling, and selected-sample peak scaling. It writes the two source panels
// separately; the Python presentation script arranges them horizontally.

#include <TCanvas.h>
#include <TChain.h>
#include <TDirectory.h>
#include <TError.h>
#include <TH1F.h>
#include <TLegend.h>
#include <TROOT.h>
#include <TString.h>
#include <TSystem.h>

#include <iostream>
#include <vector>

namespace {

std::vector<TString> HistoricalRunFiles(const TString &input_dir) {
  std::vector<TString> paths;
  paths.push_back(input_dir + "/a20240304_SnSn_GOAL_ALLCOIN.006.root");
  for (int index = 0; index <= 7; ++index) {
    paths.push_back(input_dir + Form("/a20240305_SnSn_GOAL_ALLCOIN.00%d.root", index));
  }
  for (int index = 0; index <= 14; ++index) {
    paths.push_back(input_dir + Form("/a20240306_SnSn_GOAL_ALLCOIN.0%02d.root", index));
  }
  for (int index = 0; index <= 13; ++index) {
    paths.push_back(input_dir + Form("/a20240307_SnSn_GOAL_ALLCOIN.0%02d.root", index));
  }
  for (int index = 0; index <= 10; ++index) {
    paths.push_back(input_dir + Form("/a20240308_SnSn_GOAL_ALLCOIN.0%02d.root", index));
  }
  for (int index = 0; index <= 3; ++index) {
    paths.push_back(input_dir + Form("/a20240309_SnSn_GOAL_ALLCOIN.0%02d.root", index));
  }
  for (int index = 0; index <= 6; ++index) {
    paths.push_back(input_dir + Form("/a20240310_SnSn_GOAL_ALLCOIN.0%02d.root", index));
  }
  return paths;
}

void StyleHistogram(TH1 *histogram, const char *x_title) {
  histogram->SetStats(false);
  histogram->SetTitle("");
  histogram->GetXaxis()->SetTitle(x_title);
  histogram->GetYaxis()->SetTitle("Counts");
  histogram->GetXaxis()->CenterTitle();
  histogram->GetYaxis()->CenterTitle();
  histogram->GetXaxis()->SetNdivisions(505);
  histogram->GetYaxis()->SetNdivisions(505);
  histogram->GetXaxis()->SetTitleSize(0.055);
  histogram->GetYaxis()->SetTitleSize(0.055);
  histogram->GetXaxis()->SetLabelSize(0.045);
  histogram->GetYaxis()->SetLabelSize(0.045);
  histogram->GetYaxis()->SetTitleOffset(1.25);
  histogram->SetLineWidth(2);
}

void ConfigureCanvas(TCanvas &canvas) {
  canvas.SetLeftMargin(0.16);
  canvas.SetBottomMargin(0.15);
  canvas.SetRightMargin(0.05);
  canvas.SetTopMargin(0.06);
  canvas.SetTicks(1, 1);
}

}  // namespace

void draw_unit_time_and_neighbor_difference(
    const char *analysis_root,
    const char *output_dir = "results/figures/unit_time_difference/root") {
  gROOT->SetBatch(true);

  const TString input_dir =
      TString(analysis_root) + "/DataPreprocessing/step4-convert.0308.PreRun";
  const std::vector<TString> input_paths = HistoricalRunFiles(input_dir);
  if (input_paths.size() != 60) {
    Error("draw_unit_time_and_neighbor_difference", "Expected 60 input files.");
    return;
  }

  TChain chain("GammaCaliData");
  for (const TString &path : input_paths) {
    if (gSystem->AccessPathName(path)) {
      Error("draw_unit_time_and_neighbor_difference", "Missing input: %s", path.Data());
      return;
    }
    if (chain.Add(path) != 1) {
      Error("draw_unit_time_and_neighbor_difference", "Cannot add input: %s", path.Data());
      return;
    }
  }
  if (!chain.GetBranch("GammaTime") || !chain.GetBranch("GammaEnergy")) {
    Error("draw_unit_time_and_neighbor_difference",
          "GammaCaliData must provide GammaTime and GammaEnergy.");
    return;
  }

  gROOT->cd();
  chain.Draw("GammaTime[5]>>h_unit05_time(100,-500,500)", "", "goff");
  chain.Draw(
      "GammaTime[5]-GammaTime[6]>>h_neighbor_difference_all(100,-200,200)",
      "", "goff");
  chain.Draw(
      "GammaTime[5]-GammaTime[6]>>h_neighbor_difference_cut(100,-200,200)",
      "GammaEnergy[5]+GammaEnergy[6]>=30", "goff");

  TH1F *time_5 = dynamic_cast<TH1F *>(gDirectory->Get("h_unit05_time"));
  TH1F *difference_all =
      dynamic_cast<TH1F *>(gDirectory->Get("h_neighbor_difference_all"));
  TH1F *difference_cut =
      dynamic_cast<TH1F *>(gDirectory->Get("h_neighbor_difference_cut"));
  if (!time_5 || !difference_all || !difference_cut) {
    Error("draw_unit_time_and_neighbor_difference", "ROOT did not create all histograms.");
    return;
  }
  if (difference_cut->GetMaximum() <= 0.0) {
    Error("draw_unit_time_and_neighbor_difference", "Selected histogram is empty.");
    return;
  }

  const double peak_scale =
      difference_all->GetMaximum() / difference_cut->GetMaximum();
  difference_cut->Scale(peak_scale);

  if (gSystem->mkdir(output_dir, true) != 0 && gSystem->AccessPathName(output_dir)) {
    Error("draw_unit_time_and_neighbor_difference",
          "Cannot create output directory: %s", output_dir);
    return;
  }

  TCanvas time_canvas("time_canvas", "CsI05 corrected time", 800, 600);
  ConfigureCanvas(time_canvas);
  StyleHistogram(time_5, "t_{5} (ns)");
  time_5->SetLineColor(kBlue + 1);
  time_5->Draw("hist");
  time_canvas.SaveAs(TString(output_dir) + "/cshine_gamma_unit05_time_root.pdf");

  TCanvas difference_canvas(
      "difference_canvas", "CsI05-CsI06 corrected-time difference", 800, 600);
  ConfigureCanvas(difference_canvas);
  StyleHistogram(difference_all, "t_{5}-t_{6} (ns)");
  StyleHistogram(difference_cut, "t_{5}-t_{6} (ns)");
  difference_all->SetLineColor(kBlue + 1);
  difference_cut->SetLineColor(kRed + 1);
  difference_all->GetYaxis()->SetRangeUser(-200.0, 6300.0);
  difference_all->Draw("hist");
  difference_cut->Draw("hist same");

  TLegend legend(0.20, 0.58, 0.51, 0.86);
  legend.SetBorderSize(0);
  legend.SetFillStyle(0);
  legend.AddEntry(difference_all, "No energy cut", "l");
  legend.AddEntry(difference_cut, "E_{5}+E_{6} #geq 30 MeV", "l");
  legend.Draw();
  difference_canvas.SaveAs(
      TString(output_dir) + "/cshine_gamma_neighbor_time_difference_root.pdf");

  std::cout << "Energy-selected peak scale factor: " << peak_scale << std::endl;
}

