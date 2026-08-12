// Portable ROOT reference for the reconstructed-energy--core-time figure.
//
// Historical sources:
//   DataPreprocessing/step7-DeltaYrelated/h2_check.C
//   DataPreprocessing/step7-DeltaYrelated/Drawhistos.C
//
// This macro reads the published analysis object without reconstructing
// events, rebinning, normalization, or fitting.

#include <TCanvas.h>
#include <TError.h>
#include <TFile.h>
#include <TH2.h>
#include <TROOT.h>
#include <TString.h>
#include <TSystem.h>

namespace {

bool HasPublishedBinning(const TH2 &histogram) {
  return histogram.GetNbinsX() == 100 && histogram.GetNbinsY() == 200 &&
         histogram.GetXaxis()->GetXmin() == -500.0 &&
         histogram.GetXaxis()->GetXmax() == 500.0 &&
         histogram.GetYaxis()->GetXmin() == 0.0 &&
         histogram.GetYaxis()->GetXmax() == 200.0;
}

}  // namespace

void draw_energy_core_time_correlation(
    const char *input_root,
    const char *output_dir =
        "results/figures/energy_core_time_correlation/root",
    bool force = false) {
  gROOT->SetBatch(true);

  TFile input(input_root, "READ");
  if (input.IsZombie()) {
    Error("draw_energy_core_time_correlation",
          "Cannot open input ROOT file: %s", input_root);
    return;
  }

  TH2 *histogram = nullptr;
  input.GetObject("ALL_h2_TOF_TotalE", histogram);
  if (!histogram) {
    Error("draw_energy_core_time_correlation",
          "Missing TH2 object: ALL_h2_TOF_TotalE");
    return;
  }
  if (!HasPublishedBinning(*histogram)) {
    Error("draw_energy_core_time_correlation",
          "Unexpected ALL_h2_TOF_TotalE binning.");
    return;
  }
  if (histogram->GetMaximum() <= 0.0) {
    Error("draw_energy_core_time_correlation", "Source histogram is empty.");
    return;
  }

  if (gSystem->mkdir(output_dir, true) != 0 &&
      gSystem->AccessPathName(output_dir)) {
    Error("draw_energy_core_time_correlation",
          "Cannot create output directory: %s", output_dir);
    return;
  }

  const TString output =
      TString(output_dir) + "/energy_core_time_correlation_root.pdf";
  if (!force && !gSystem->AccessPathName(output)) {
    Error("draw_energy_core_time_correlation",
          "Output exists; pass force=true to replace it.");
    return;
  }

  TCanvas canvas("energy_core_time_canvas", "energy versus core time", 800,
                 600);
  canvas.SetLeftMargin(0.15);
  canvas.SetBottomMargin(0.18);
  canvas.SetRightMargin(0.13);
  canvas.SetTopMargin(0.06);
  canvas.SetLogz();
  canvas.SetTicks(1, 1);

  histogram->SetStats(false);
  histogram->SetTitle(";T_{core} (ns);E_{tot} (MeV)");
  histogram->GetXaxis()->CenterTitle();
  histogram->GetYaxis()->CenterTitle();
  histogram->GetXaxis()->SetNdivisions(505);
  histogram->GetYaxis()->SetNdivisions(505);
  histogram->GetXaxis()->SetTitleSize(0.08);
  histogram->GetYaxis()->SetTitleSize(0.08);
  histogram->GetXaxis()->SetLabelSize(0.07);
  histogram->GetYaxis()->SetLabelSize(0.07);
  histogram->GetYaxis()->SetTitleOffset(0.8);
  histogram->Draw("COLZ");
  canvas.SaveAs(output);
}
