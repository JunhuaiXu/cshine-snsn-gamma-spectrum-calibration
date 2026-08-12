// Portable ROOT reference for analysis-note Fig. 18.
//
// Final-selection producer:
//   DataPreprocessing/step7-DeltaYrelated/h2_check_BKG.C
// Historical drawing reference:
//   DataPreprocessing/step11-otherFigsLongPaper/
//     Fig2-deltaxdeltayBKG/Drawhistos.C
//
// The producer uses the same central/side-core and plastic-veto selection as
// analysis-note Fig. 10. The drawing macro reads the two beam-off ROOT objects
// without rebinning or normalization and uses independent logarithmic count
// scales. This focused reference writes the panels separately; the Python
// script arranges the same objects horizontally.

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
    Error("draw_cosmic_muon_topology", "Missing TH2 object: %s", name);
  }
  return histogram;
}

bool HasBinning(const TH2 &histogram, int x_bins, double x_min,
                double x_max, int y_bins, double y_min, double y_max) {
  return histogram.GetNbinsX() == x_bins &&
         histogram.GetNbinsY() == y_bins &&
         histogram.GetXaxis()->GetXmin() == x_min &&
         histogram.GetXaxis()->GetXmax() == x_max &&
         histogram.GetYaxis()->GetXmin() == y_min &&
         histogram.GetYaxis()->GetXmax() == y_max;
}

void ConfigureCanvas(TCanvas &canvas) {
  canvas.SetLeftMargin(0.15);
  canvas.SetBottomMargin(0.18);
  canvas.SetRightMargin(0.13);
  canvas.SetTopMargin(0.06);
  canvas.SetLogz();
  canvas.SetTicks(1, 1);
}

void StyleHistogram(TH2 &histogram, const char *title) {
  histogram.SetStats(false);
  histogram.SetTitle(title);
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

void draw_cosmic_muon_topology(
    const char *input_root,
    const char *output_dir =
        "results/figures/cosmic_muon_topology/root",
    bool force = false) {
  gROOT->SetBatch(true);

  TFile input(input_root, "READ");
  if (input.IsZombie()) {
    Error("draw_cosmic_muon_topology", "Cannot open input ROOT file: %s",
          input_root);
    return;
  }

  TH2 *spatial = RequireHistogram(input, "ALL_h2_ax_ay_100_inf");
  TH2 *energy = RequireHistogram(input, "ALL_h2_TotalE_CenterE");
  if (!spatial || !energy) {
    return;
  }
  if (!HasBinning(*spatial, 70, 0.0, 7.0, 70, 0.0, 7.0) ||
      !HasBinning(*energy, 50, 5.0, 200.0, 80, 0.0, 80.0)) {
    Error("draw_cosmic_muon_topology",
          "Input does not match the frozen final-selection histogram binning.");
    return;
  }
  if (spatial->GetMaximum() <= 0.0 || energy->GetMaximum() <= 0.0) {
    Error("draw_cosmic_muon_topology", "At least one histogram is empty.");
    return;
  }

  if (gSystem->mkdir(output_dir, true) != 0 &&
      gSystem->AccessPathName(output_dir)) {
    Error("draw_cosmic_muon_topology", "Cannot create output directory: %s",
          output_dir);
    return;
  }

  const TString spatial_output =
      TString(output_dir) + "/beam_off_delta_y_vs_delta_x_root.pdf";
  const TString energy_output =
      TString(output_dir) + "/beam_off_core_vs_total_energy_root.pdf";
  if (!force && (!gSystem->AccessPathName(spatial_output) ||
                 !gSystem->AccessPathName(energy_output))) {
    Error("draw_cosmic_muon_topology",
          "Output exists; pass force=true to replace it.");
    return;
  }

  TCanvas spatial_canvas("spatial_canvas", "beam-off spatial topology", 800,
                         600);
  ConfigureCanvas(spatial_canvas);
  StyleHistogram(*spatial, ";#delta_{x} (cm);#delta_{y} (cm)");
  spatial->Draw("COLZ");
  spatial_canvas.SaveAs(spatial_output);

  TCanvas energy_canvas("energy_canvas", "beam-off energy sharing", 800, 600);
  ConfigureCanvas(energy_canvas);
  StyleHistogram(*energy, ";E_{tot} (MeV);E_{core} (MeV)");
  energy->Draw("COLZ");
  energy_canvas.SaveAs(energy_output);
}
