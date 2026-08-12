// Horizontal ROOT rendering of the two frozen event-display records.
//
// The input ROOT file is produced by
//   analysis/data_preprocessing/validation/build_event_display_records.cxx
// and contains the event selections.  This macro changes only the layout and
// does not repeat any event selection or reconstruction.

#include <TCanvas.h>
#include <TError.h>
#include <TFile.h>
#include <TH2.h>
#include <TLatex.h>
#include <TPad.h>
#include <TROOT.h>
#include <TString.h>
#include <TStyle.h>
#include <TSystem.h>

namespace {

TH2* RequireHistogram(TFile& input, const char* name) {
  TH2* histogram = nullptr;
  input.GetObject(name, histogram);
  if (!histogram) {
    Error("draw_event_display_horizontal", "Missing TH2 object: %s", name);
  }
  return histogram;
}

void ConfigureHistogram(TH2& histogram) {
  histogram.SetStats(false);
  histogram.SetTitle("");
  histogram.GetXaxis()->SetTitle("Row Index");
  histogram.GetYaxis()->SetTitle("Column Index");
  histogram.GetZaxis()->SetTitle("Deposited Energy [MeV]");
  histogram.GetXaxis()->CenterTitle();
  histogram.GetYaxis()->CenterTitle();
  histogram.GetZaxis()->CenterTitle();
  histogram.GetXaxis()->SetNdivisions(202);
  histogram.GetYaxis()->SetNdivisions(202);
  histogram.GetZaxis()->SetNdivisions(502);
  for (int bin = 1; bin <= 4; ++bin) {
    histogram.GetXaxis()->SetBinLabel(bin, Form("%d", bin));
    histogram.GetYaxis()->SetBinLabel(bin, Form("%d", bin));
  }
  histogram.GetXaxis()->SetLabelSize(0.040);
  histogram.GetYaxis()->SetLabelSize(0.040);
  histogram.GetZaxis()->SetLabelSize(0.038);
  histogram.GetXaxis()->SetTitleSize(0.044);
  histogram.GetYaxis()->SetTitleSize(0.044);
  histogram.GetZaxis()->SetTitleSize(0.036);
  histogram.GetXaxis()->SetTitleOffset(1.25);
  histogram.GetYaxis()->SetTitleOffset(1.25);
  histogram.GetZaxis()->SetTitleOffset(1.35);
  histogram.SetMinimum(0.0);
  histogram.SetMaximum(100.0);
}

void ConfigurePad(TVirtualPad& pad) {
  pad.SetLeftMargin(0.19);
  pad.SetRightMargin(0.02);
  pad.SetBottomMargin(0.08);
  pad.SetTopMargin(0.04);
  pad.SetTheta(60.0);
  pad.SetPhi(30.0);
  pad.SetTicks(1, 1);
}

void DrawPanel(TVirtualPad& pad, TH2& histogram, const char* label) {
  pad.cd();
  ConfigurePad(pad);
  ConfigureHistogram(histogram);
  histogram.Draw("LEGO2");
  pad.Update();
  TLatex panel_label;
  panel_label.SetNDC(true);
  panel_label.SetTextFont(62);
  panel_label.SetTextSize(0.055);
  panel_label.DrawLatex(0.035, 0.91, label);
  pad.Modified();
}

}  // namespace

void draw_event_display_horizontal(
    const char* input_root,
    const char* output_dir = "results/figures/event_display/root",
    bool force = false) {
  gROOT->SetBatch(true);
  gStyle->SetPalette(kRainBow);
  gStyle->SetFrameBorderMode(0);
  gStyle->SetFrameLineWidth(0);

  TFile input(input_root, "READ");
  if (input.IsZombie()) {
    Error("draw_event_display_horizontal", "Cannot open input: %s", input_root);
    return;
  }
  TH2* gamma = RequireHistogram(input, "gamma_event_display");
  TH2* cosmic = RequireHistogram(input, "cosmic_event_display");
  if (!gamma || !cosmic) {
    return;
  }
  if (gamma->GetNbinsX() != 4 || gamma->GetNbinsY() != 4 ||
      cosmic->GetNbinsX() != 4 || cosmic->GetNbinsY() != 4) {
    Error("draw_event_display_horizontal",
          "Both event-display histograms must use the frozen 4x4 geometry.");
    return;
  }
  if (gSystem->mkdir(output_dir, true) != 0 &&
      gSystem->AccessPathName(output_dir)) {
    Error("draw_event_display_horizontal", "Cannot create output directory: %s",
          output_dir);
    return;
  }

  const TString pdf = TString(output_dir) +
                      "/cshine_gamma_event_display_horizontal.pdf";
  const TString png = TString(output_dir) +
                      "/cshine_gamma_event_display_horizontal.png";
  if (!force && (!gSystem->AccessPathName(pdf) ||
                 !gSystem->AccessPathName(png))) {
    Error("draw_event_display_horizontal",
          "Output exists; pass force=true to replace it.");
    return;
  }

  TCanvas canvas("event_display_canvas", "event displays", 1600, 680);
  canvas.Divide(2, 1, 0.015, 0.0);
  DrawPanel(*canvas.GetPad(1), *gamma, "(a)");
  DrawPanel(*canvas.GetPad(2), *cosmic, "(b)");
  canvas.SaveAs(pdf);
  canvas.SaveAs(png);
}
