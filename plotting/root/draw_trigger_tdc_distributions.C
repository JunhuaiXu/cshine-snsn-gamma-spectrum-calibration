// Portable ROOT reference for the monitored-trigger TDC distributions.
//
// Historical sources:
//   DataPreprocessing/step8-TimeCheck/step6-TimeWalkPlot/
//     step7-DeltaYrelated/checkTrig.C
//   DataPreprocessing/step8-TimeCheck/step6-TimeWalkPlot/
//     step7-DeltaYrelated/Drawhistos.C
//   DataPreprocessing/step8-TimeCheck/step6-TimeWalkPlot/
//     step7-DeltaYrelated/TriggerTDC.ipynb
//
// The ROOT producer allocates h1_TrigList0--14, while the historical trigger
// drawing reads only objects 0--6.  This reference preserves the published
// six-panel order and adds only panel/trigger annotations.

#include <array>

#include <TCanvas.h>
#include <TError.h>
#include <TFile.h>
#include <TH1.h>
#include <TLatex.h>
#include <TROOT.h>
#include <TString.h>
#include <TSystem.h>

namespace {

struct TriggerPanel {
  int histogram_index;
  const char *panel;
  const char *label;
};

constexpr std::array<TriggerPanel, 6> kPanels{{
    {1, "(a)", "SSD M1 & CsI M1"},
    {2, "(b)", "SSD M2"},
    {3, "(c)", "SSD M1 & NA M1"},
    {4, "(d)", "NA M1 & T_{0}"},
    {6, "(e)", "LS & T_{0}"},
    {0, "(f)", "ALL_{OR} (global trigger)"},
}};

bool HasHistoricalBinning(const TH1 &histogram) {
  return histogram.GetNbinsX() == 4096 &&
         histogram.GetXaxis()->GetXmin() == 0.0 &&
         histogram.GetXaxis()->GetXmax() == 4096.0;
}

}  // namespace

void draw_trigger_tdc_distributions(
    const char *input_root,
    const char *output_dir =
        "results/figures/trigger_tdc_distributions/root",
    bool force = false) {
  gROOT->SetBatch(true);

  TFile input(input_root, "READ");
  if (input.IsZombie()) {
    Error("draw_trigger_tdc_distributions",
          "Cannot open input ROOT file: %s", input_root);
    return;
  }

  if (gSystem->mkdir(output_dir, true) != 0 &&
      gSystem->AccessPathName(output_dir)) {
    Error("draw_trigger_tdc_distributions",
          "Cannot create output directory: %s", output_dir);
    return;
  }

  const TString output =
      TString(output_dir) + "/trigger_tdc_distributions_root.pdf";
  if (!force && !gSystem->AccessPathName(output)) {
    Error("draw_trigger_tdc_distributions",
          "Output exists; pass force=true to replace it.");
    return;
  }

  std::array<TH1 *, 6> histograms{};
  for (std::size_t index = 0; index < kPanels.size(); ++index) {
    input.GetObject(Form("h1_TrigList%d", kPanels[index].histogram_index),
                    histograms[index]);
    if (!histograms[index]) {
      Error("draw_trigger_tdc_distributions", "Missing ROOT object for %s",
            kPanels[index].label);
      return;
    }
    if (!HasHistoricalBinning(*histograms[index])) {
      Error("draw_trigger_tdc_distributions",
            "Unexpected TDC binning for h1_TrigList%d",
            kPanels[index].histogram_index);
      return;
    }
  }

  TCanvas canvas("trigger_tdc_canvas", "trigger TDC distributions", 1200,
                 800);
  canvas.Divide(3, 2, 0.001, 0.001);

  for (std::size_t index = 0; index < kPanels.size(); ++index) {
    canvas.cd(index + 1);
    gPad->SetLogy();
    gPad->SetTicks(1, 1);
    gPad->SetLeftMargin(index % 3 == 0 ? 0.16 : 0.10);
    gPad->SetRightMargin(0.03);
    gPad->SetTopMargin(0.04);
    gPad->SetBottomMargin(index >= 3 ? 0.17 : 0.08);

    TH1 *histogram = histograms[index];
    histogram->SetStats(false);
    histogram->SetTitle(";TDC channel;Counts");
    histogram->SetLineColor(kBlack);
    histogram->SetLineWidth(1);
    histogram->SetMinimum(1.0);
    histogram->SetMaximum(1.0e8);
    histogram->GetXaxis()->SetRangeUser(0.0, 4096.0);
    histogram->Draw("HIST");

    TLatex annotation;
    annotation.SetNDC(true);
    annotation.SetTextFont(62);
    annotation.SetTextSize(0.065);
    annotation.SetTextAlign(13);
    annotation.DrawLatex(0.05, 0.92, kPanels[index].panel);
    annotation.SetTextAlign(33);
    annotation.SetTextSize(0.052);
    annotation.DrawLatex(0.96, 0.88, kPanels[index].label);
  }

  canvas.SaveAs(output);
}
