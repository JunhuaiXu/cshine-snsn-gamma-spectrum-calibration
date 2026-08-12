// Portable ROOT reference for analysis-note Fig. 11.
//
// Historical sources:
//   DataPreprocessing/step7-DeltaYrelated/h2_check.C
//   DataPreprocessing/step7-DeltaYrelated/Drawhistos.C
//
// The source analysis fills the two ROOT objects. Drawhistos.C applies no
// normalization and draws each object separately with a logarithmic count
// scale. This macro preserves that output-layer behavior. The Python entry
// arranges the same objects horizontally without changing their binning.

#include <TCanvas.h>
#include <TError.h>
#include <TFile.h>
#include <TH2.h>
#include <TROOT.h>
#include <TString.h>
#include <TSystem.h>

namespace {

TH2 *RequireHistogram(TFile &input, const char *name) {
  TH2 *histogram = nullptr;
  input.GetObject(name, histogram);
  if (!histogram) {
    Error("draw_energy_spatial_spread_correlations",
          "Missing TH2 object: %s", name);
  }
  return histogram;
}

bool HasBinning(const TH2 &histogram, double x_min) {
  return histogram.GetNbinsX() == 50 && histogram.GetNbinsY() == 70 &&
         histogram.GetXaxis()->GetXmin() == x_min &&
         histogram.GetXaxis()->GetXmax() == 200.0 &&
         histogram.GetYaxis()->GetXmin() == 0.0 &&
         histogram.GetYaxis()->GetXmax() == 7.0;
}

void ConfigureCanvas(TCanvas &canvas) {
  canvas.SetLeftMargin(0.15);
  canvas.SetBottomMargin(0.18);
  canvas.SetRightMargin(0.13);
  canvas.SetTopMargin(0.06);
  canvas.SetLogz();
  canvas.SetTicks(1, 1);
}

void StyleHistogram(TH2 &histogram, const char *y_title) {
  histogram.SetStats(false);
  histogram.SetTitle(Form(";E_{tot} (MeV);%s", y_title));
  histogram.GetXaxis()->CenterTitle();
  histogram.GetYaxis()->CenterTitle();
  histogram.GetXaxis()->SetNdivisions(505);
  histogram.GetYaxis()->SetNdivisions(505);
  histogram.GetXaxis()->SetTitleSize(0.08);
  histogram.GetYaxis()->SetTitleSize(0.08);
  histogram.GetXaxis()->SetLabelSize(0.07);
  histogram.GetYaxis()->SetLabelSize(0.07);
  histogram.GetYaxis()->SetTitleOffset(0.7);
}

}  // namespace

void draw_energy_spatial_spread_correlations(
    const char *input_root,
    const char *output_dir =
        "results/figures/energy_spatial_spread_correlations/root",
    bool force = false) {
  gROOT->SetBatch(true);

  TFile input(input_root, "READ");
  if (input.IsZombie()) {
    Error("draw_energy_spatial_spread_correlations",
          "Cannot open input ROOT file: %s", input_root);
    return;
  }

  TH2 *delta_y = RequireHistogram(input, "ALL_h2_TotalE_DeltaY");
  TH2 *delta_r = RequireHistogram(input, "ALL_h2_TotalE_Delta");
  if (!delta_y || !delta_r) {
    return;
  }
  if (!HasBinning(*delta_y, 5.0) || !HasBinning(*delta_r, 0.0)) {
    Error("draw_energy_spatial_spread_correlations",
          "Unexpected historical binning for delta_y or delta_r.");
    return;
  }
  if (delta_y->GetMaximum() <= 0.0 || delta_r->GetMaximum() <= 0.0) {
    Error("draw_energy_spatial_spread_correlations",
          "At least one source histogram is empty.");
    return;
  }

  if (gSystem->mkdir(output_dir, true) != 0 &&
      gSystem->AccessPathName(output_dir)) {
    Error("draw_energy_spatial_spread_correlations",
          "Cannot create output directory: %s", output_dir);
    return;
  }

  const TString delta_y_output =
      TString(output_dir) + "/total_energy_delta_y_root.pdf";
  const TString delta_r_output =
      TString(output_dir) + "/total_energy_delta_r_root.pdf";
  if (!force &&
      (!gSystem->AccessPathName(delta_y_output) ||
       !gSystem->AccessPathName(delta_r_output))) {
    Error("draw_energy_spatial_spread_correlations",
          "Output exists; pass force=true to replace it.");
    return;
  }

  TCanvas delta_y_canvas("delta_y_canvas", "E_tot versus delta_y", 800, 600);
  ConfigureCanvas(delta_y_canvas);
  StyleHistogram(*delta_y, "#delta_{y} (cm)");
  delta_y->Draw("COLZ");
  delta_y_canvas.SaveAs(delta_y_output);

  TCanvas delta_r_canvas("delta_r_canvas", "E_tot versus delta_r", 800, 600);
  ConfigureCanvas(delta_r_canvas);
  StyleHistogram(*delta_r, "#delta_{r} (cm)");
  delta_r->Draw("COLZ");
  delta_r_canvas.SaveAs(delta_r_output);
}
