// Portable ROOT reference for analysis-note Fig. 15.
//
// Historical source branches:
//   DataPreprocessing/step8-TimeCheck/step5-{onlygamma,SSDM2,NAandSSD,
//     T0andNA,T0LS}/step7-DeltaYrelated/{h2_check.C,Drawhistos.C}
//
// Each branch writes h2_check.root:ALL_h2_TOF_TotalE after applying its own
// monitored-trigger TDC validity condition upstream.  This reference reads
// those five frozen objects and adds only a common 2-row x 3-column presentation.

#include <array>

#include <TCanvas.h>
#include <TError.h>
#include <TFile.h>
#include <TH2.h>
#include <TLatex.h>
#include <TROOT.h>
#include <TString.h>
#include <TSystem.h>

namespace {

struct TriggerPanel {
  const char *branch;
  const char *panel;
  const char *label;
  int trigger_index;
};

constexpr std::array<TriggerPanel, 5> kPanels{{
    {"step5-onlygamma", "(a)", "SSD M1 & CsI M1", 17},
    {"step5-SSDM2", "(b)", "SSD M2", 18},
    {"step5-NAandSSD", "(c)", "SSD M1 & NA M1", 19},
    {"step5-T0andNA", "(d)", "NA M1 & T_{0}", 20},
    {"step5-T0LS", "(e)", "LS & T_{0}", 22},
}};

bool HasPublishedBinning(const TH2 &histogram) {
  return histogram.GetNbinsX() == 100 && histogram.GetNbinsY() == 200 &&
         histogram.GetXaxis()->GetXmin() == -500.0 &&
         histogram.GetXaxis()->GetXmax() == 500.0 &&
         histogram.GetYaxis()->GetXmin() == 0.0 &&
         histogram.GetYaxis()->GetXmax() == 200.0;
}

}  // namespace

void draw_trigger_energy_time_correlations(
    const char *analysis_root,
    const char *output_dir =
        "results/figures/trigger_energy_time_correlations/root",
    bool force = false) {
  gROOT->SetBatch(true);

  if (gSystem->mkdir(output_dir, true) != 0 &&
      gSystem->AccessPathName(output_dir)) {
    Error("draw_trigger_energy_time_correlations",
          "Cannot create output directory: %s", output_dir);
    return;
  }

  const TString output = TString(output_dir) +
                         "/trigger_energy_time_correlations_root.pdf";
  if (!force && !gSystem->AccessPathName(output)) {
    Error("draw_trigger_energy_time_correlations",
          "Output exists; pass force=true to replace it.");
    return;
  }

  std::array<TFile *, 5> inputs{};
  std::array<TH2 *, 5> histograms{};
  for (std::size_t index = 0; index < kPanels.size(); ++index) {
    const TString input_path =
        TString(analysis_root) + "/DataPreprocessing/step8-TimeCheck/" +
        kPanels[index].branch + "/step7-DeltaYrelated/h2_check.root";
    inputs[index] = TFile::Open(input_path, "READ");
    if (!inputs[index] || inputs[index]->IsZombie()) {
      Error("draw_trigger_energy_time_correlations",
            "Cannot open input ROOT file: %s", input_path.Data());
      return;
    }
    inputs[index]->GetObject("ALL_h2_TOF_TotalE", histograms[index]);
    if (!histograms[index]) {
      Error("draw_trigger_energy_time_correlations",
            "Missing ALL_h2_TOF_TotalE in %s", input_path.Data());
      return;
    }
    if (!HasPublishedBinning(*histograms[index])) {
      Error("draw_trigger_energy_time_correlations",
            "Unexpected ALL_h2_TOF_TotalE binning in %s",
            input_path.Data());
      return;
    }
    if (histograms[index]->GetMaximum() <= 0.0) {
      Error("draw_trigger_energy_time_correlations",
            "Empty ALL_h2_TOF_TotalE in %s", input_path.Data());
      return;
    }
  }

  TCanvas canvas("trigger_energy_time_canvas",
                 "trigger-conditioned energy versus core time", 1200, 800);
  canvas.Divide(3, 2, 0.001, 0.001);

  for (std::size_t index = 0; index < kPanels.size(); ++index) {
    canvas.cd(index + 1);
    gPad->SetLogz();
    gPad->SetTicks(1, 1);
    gPad->SetLeftMargin(0.15);
    gPad->SetRightMargin(0.14);
    gPad->SetTopMargin(0.06);
    gPad->SetBottomMargin(0.16);

    TH2 *histogram = histograms[index];
    histogram->SetStats(false);
    histogram->SetTitle(index % 3 == 0
                            ? ";T_{core} (ns);E_{tot} (MeV)"
                            : ";T_{core} (ns);");
    histogram->GetXaxis()->CenterTitle();
    histogram->GetYaxis()->CenterTitle();
    histogram->GetXaxis()->SetNdivisions(505);
    histogram->GetYaxis()->SetNdivisions(505);
    histogram->Draw("COLZ");

    TLatex annotation;
    annotation.SetNDC(true);
    annotation.SetTextFont(62);
    annotation.SetTextSize(0.045);
    annotation.SetTextAlign(13);
    annotation.DrawLatex(0.04, 0.95,
                         Form("%s  %s", kPanels[index].panel,
                              kPanels[index].label));
  }

  canvas.cd(6);
  gPad->Clear();
  canvas.SaveAs(output);

  for (TFile *input : inputs) {
    if (input) {
      input->Close();
      delete input;
    }
  }
}
