// Portable ROOT reference for the E_core--E_tot source panels.
//
// Historical sources:
//   DataPreprocessing/step7-DeltaYrelated/h2_check.C
//   DataPreprocessing/step7-DeltaYrelated/Drawhistos.C
//
// The historical analysis fills central and side-core histograms separately.
// Drawhistos.C draws each without rebinning or normalization and uses an
// independent logarithmic count scale. This macro preserves that output layer.

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
    Error("draw_core_total_energy_correlations",
          "Missing TH2 object: %s", name);
  }
  return histogram;
}

bool HasHistoricalBinning(const TH2 &histogram) {
  return histogram.GetNbinsX() == 50 && histogram.GetNbinsY() == 80 &&
         histogram.GetXaxis()->GetXmin() == 5.0 &&
         histogram.GetXaxis()->GetXmax() == 200.0 &&
         histogram.GetYaxis()->GetXmin() == 0.0 &&
         histogram.GetYaxis()->GetXmax() == 80.0;
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
  histogram.SetTitle(";E_{tot} (MeV);E_{core} (MeV)");
  histogram.GetXaxis()->CenterTitle();
  histogram.GetYaxis()->CenterTitle();
  histogram.GetXaxis()->SetNdivisions(505);
  histogram.GetYaxis()->SetNdivisions(505);
  histogram.GetXaxis()->SetTitleSize(0.08);
  histogram.GetYaxis()->SetTitleSize(0.08);
  histogram.GetXaxis()->SetLabelSize(0.07);
  histogram.GetYaxis()->SetLabelSize(0.07);
  histogram.GetYaxis()->SetTitleOffset(0.8);
}

}  // namespace

void draw_core_total_energy_correlations(
    const char *input_root,
    const char *output_dir =
        "results/figures/core_total_energy_correlations/root",
    bool force = false) {
  gROOT->SetBatch(true);

  TFile input(input_root, "READ");
  if (input.IsZombie()) {
    Error("draw_core_total_energy_correlations",
          "Cannot open input ROOT file: %s", input_root);
    return;
  }

  TH2 *central =
      RequireHistogram(input, "central_h2_TotalE_CenterE");
  TH2 *side = RequireHistogram(input, "side_h2_TotalE_CenterE");
  if (!central || !side) {
    return;
  }
  if (!HasHistoricalBinning(*central) || !HasHistoricalBinning(*side)) {
    Error("draw_core_total_energy_correlations",
          "Unexpected historical E_tot--E_core binning.");
    return;
  }
  if (central->GetMaximum() <= 0.0 || side->GetMaximum() <= 0.0) {
    Error("draw_core_total_energy_correlations",
          "At least one source histogram is empty.");
    return;
  }

  if (gSystem->mkdir(output_dir, true) != 0 &&
      gSystem->AccessPathName(output_dir)) {
    Error("draw_core_total_energy_correlations",
          "Cannot create output directory: %s", output_dir);
    return;
  }

  const TString central_output =
      TString(output_dir) + "/central_core_total_energy_root.pdf";
  const TString side_output =
      TString(output_dir) + "/side_core_total_energy_root.pdf";
  if (!force &&
      (!gSystem->AccessPathName(central_output) ||
       !gSystem->AccessPathName(side_output))) {
    Error("draw_core_total_energy_correlations",
          "Output exists; pass force=true to replace it.");
    return;
  }

  TCanvas central_canvas("central_core_canvas", "central core", 800, 600);
  ConfigureCanvas(central_canvas);
  StyleHistogram(*central);
  central->Draw("COLZ");
  central_canvas.SaveAs(central_output);

  TCanvas side_canvas("side_core_canvas", "side core", 800, 600);
  ConfigureCanvas(side_canvas);
  StyleHistogram(*side);
  side->Draw("COLZ");
  side_canvas.SaveAs(side_output);
}
