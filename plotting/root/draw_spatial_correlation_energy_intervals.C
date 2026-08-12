// Portable ROOT reference for analysis-note Fig. 10.
//
// Historical sources:
//   DataPreprocessing/step7-DeltaYrelated/h2_check.C
//   DataPreprocessing/step7-DeltaYrelated/Drawhistos.C
//
// h2_check.C defines and fills the two ROOT objects. Drawhistos.C reads the
// same objects, applies no normalization, and draws each with a logarithmic
// count scale. This focused macro preserves that output-layer behavior and
// writes the two source panels separately. The Python presentation script
// arranges the same objects horizontally.

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
    Error("draw_spatial_correlation_energy_intervals",
          "Missing TH2 object: %s", name);
  }
  return histogram;
}

bool HasHistoricalBinning(const TH2 &histogram) {
  return histogram.GetNbinsX() == 70 && histogram.GetNbinsY() == 70 &&
         histogram.GetXaxis()->GetXmin() == 0.0 &&
         histogram.GetXaxis()->GetXmax() == 7.0 &&
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

void StyleHistogram(TH2 &histogram) {
  histogram.SetStats(false);
  histogram.SetTitle(";#delta_{x} (cm);#delta_{y} (cm)");
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

void draw_spatial_correlation_energy_intervals(
    const char *input_root,
    const char *output_dir =
        "results/figures/spatial_correlation_energy_intervals/root",
    bool force = false) {
  gROOT->SetBatch(true);

  TFile input(input_root, "READ");
  if (input.IsZombie()) {
    Error("draw_spatial_correlation_energy_intervals",
          "Cannot open input ROOT file: %s", input_root);
    return;
  }

  TH2 *low_energy = RequireHistogram(input, "ALL_h2_ax_ay_10_100");
  TH2 *high_energy = RequireHistogram(input, "ALL_h2_ax_ay_100_inf");
  if (!low_energy || !high_energy) {
    return;
  }
  if (!HasHistoricalBinning(*low_energy) ||
      !HasHistoricalBinning(*high_energy)) {
    Error("draw_spatial_correlation_energy_intervals",
          "Expected 70 x 70 bins over [0, 7] cm on both axes.");
    return;
  }
  if (low_energy->GetMaximum() <= 0.0 || high_energy->GetMaximum() <= 0.0) {
    Error("draw_spatial_correlation_energy_intervals",
          "At least one source histogram is empty.");
    return;
  }

  if (gSystem->mkdir(output_dir, true) != 0 &&
      gSystem->AccessPathName(output_dir)) {
    Error("draw_spatial_correlation_energy_intervals",
          "Cannot create output directory: %s", output_dir);
    return;
  }

  const TString low_output =
      TString(output_dir) + "/spatial_correlation_10_100MeV_root.pdf";
  const TString high_output =
      TString(output_dir) + "/spatial_correlation_above_100MeV_root.pdf";
  if (!force &&
      (!gSystem->AccessPathName(low_output) ||
       !gSystem->AccessPathName(high_output))) {
    Error("draw_spatial_correlation_energy_intervals",
          "Output exists; pass force=true to replace it.");
    return;
  }

  TCanvas low_canvas("low_canvas", "10 <= E_tot <= 100 MeV", 800, 600);
  ConfigureCanvas(low_canvas);
  StyleHistogram(*low_energy);
  low_energy->Draw("COLZ");
  low_canvas.SaveAs(low_output);

  TCanvas high_canvas("high_canvas", "E_tot > 100 MeV", 800, 600);
  ConfigureCanvas(high_canvas);
  StyleHistogram(*high_energy);
  high_energy->Draw("COLZ");
  high_canvas.SaveAs(high_output);
}
