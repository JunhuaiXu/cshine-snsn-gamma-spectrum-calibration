#include "gain_relation.h"

#include "t_2d_fit.h"

#include <TCanvas.h>
#include <TFile.h>
#include <TGraph.h>
#include <TPad.h>
#include <TROOT.h>
#include <TSystem.h>
#include <TTree.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

bool NearlyEqual(double left, double right, double tolerance) {
  return std::abs(left - right) <= tolerance;
}

bool Check(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAILED: " << message << '\n';
  }
  return condition;
}

void FillEvent(TTree& tree,
               std::array<UShort_t, 15>& low,
               std::array<UShort_t, 15>& high,
               UShort_t high_value) {
  for (unsigned int channel = 0; channel < 15; ++channel) {
    high[channel] = high_value;
    low[channel] = static_cast<UShort_t>(60 + channel + high_value / 10);
  }
  tree.Fill();
}

void WriteSyntheticInput(const std::string& path) {
  TFile output(path.c_str(), "RECREATE");
  if (output.IsZombie()) {
    throw std::runtime_error("cannot create synthetic gain input");
  }
  TTree tree("tree", "");
  std::array<UShort_t, 15> low = {};
  std::array<UShort_t, 15> high = {};
  std::array<std::string, 15> low_names;
  std::array<std::string, 15> high_names;
  std::array<std::string, 15> low_leaves;
  std::array<std::string, 15> high_leaves;
  for (unsigned int channel = 0; channel < 15; ++channel) {
    low_names[channel] = cshine_gamma::LowGainBranchName(channel);
    high_names[channel] = cshine_gamma::GainHighGainBranchName(channel);
    low_leaves[channel] = low_names[channel] + "/s";
    high_leaves[channel] = high_names[channel] + "/s";
    tree.Branch(low_names[channel].c_str(),
                &low[channel],
                low_leaves[channel].c_str());
    tree.Branch(high_names[channel].c_str(),
                &high[channel],
                high_leaves[channel].c_str());
  }

  FillEvent(tree, low, high, 950);
  for (UShort_t high_value = 1000; high_value <= 3500;
       high_value = static_cast<UShort_t>(high_value + 10)) {
    FillEvent(tree, low, high, high_value);
  }
  FillEvent(tree, low, high, 4000);

  for (unsigned int channel = 0; channel < 15; ++channel) {
    high[channel] = 1200;
    low[channel] = 150;
  }
  tree.Fill();
  for (unsigned int channel = 0; channel < 15; ++channel) {
    high[channel] = 1200;
    low[channel] = 600;
  }
  tree.Fill();

  tree.Write();
  output.Close();
}

std::string ReadTextFile(const std::string& path) {
  std::ifstream input(path.c_str());
  std::ostringstream content;
  content << input.rdbuf();
  return content.str();
}

}  // namespace

int main() {
  gROOT->SetBatch(kTRUE);
  bool ok = true;

  const cshine_gamma::GainRelationDefinition central =
      cshine_gamma::Central0308GainRelationDefinition();
  ok &= Check(central.first_file == 0 && central.last_file == 104,
              "central 105-file range");
  ok &= Check(central.channel_count == 15,
              "central 15-channel definition");
  ok &= Check(cshine_gamma::GainSelection(central, 0) ==
                  "GAMMA1_LOW_E<600&&GAMMA1_LOW_E>150",
              "strict historical low-gain selection");
  ok &= Check(NearlyEqual(central.fit_high_gain_min, 1000.0, 0.0) &&
                  NearlyEqual(central.fit_high_gain_max, 3500.0, 0.0) &&
                  NearlyEqual(central.slope_min, 0.099, 0.0) &&
                  NearlyEqual(central.slope_max, 0.101, 0.0),
              "historical fit range and slope limits");
  ok &= Check(cshine_gamma::HistoricalGainCanvasPad(12) == 1 &&
                  cshine_gamma::HistoricalGainCanvasPad(0) == 16 &&
                  cshine_gamma::HistoricalGainCanvasPad(14) == 2,
              "historical 4-by-4 crystal layout");
  ok &= Check(cshine_gamma::FormatGainInputFilename(
                  "/analysis/raw", central.input_prefix, 104) ==
                  "/analysis/raw/a20240308_SnSn_GOAL_ALLCOIN.0104.root",
              "historical input filename format");

  std::ostringstream directory_name;
  directory_name << gSystem->TempDirectory() << "/cshine_gamma_gain_relation_"
                 << gSystem->GetPid();
  const std::string directory = directory_name.str();
  gSystem->mkdir(directory.c_str(), true);

  cshine_gamma::GainRelationDefinition synthetic = central;
  synthetic.name = "synthetic";
  synthetic.input_prefix = "SYNTH_GAIN";
  synthetic.first_file = 0;
  synthetic.last_file = 0;
  synthetic.output_object_title = "synthetic_gain.root";
  synthetic.canvas_title = "synthetic gain relation";

  const std::string input_path = cshine_gamma::FormatGainInputFilename(
      directory, synthetic.input_prefix, 0);
  const std::string output_directory = directory + "/nested-results/m3";
  const std::string output_path = output_directory + "/gain.root";
  const std::string parameter_path = output_directory + "/gain.parameters.txt";
  const std::string report_path = output_directory + "/gain.run.tsv";
  WriteSyntheticInput(input_path);

  const cshine_gamma::GainRelationSummary summary =
      cshine_gamma::FitGainRelation(synthetic,
                                    directory,
                                    output_path,
                                    parameter_path,
                                    report_path,
                                    std::string(),
                                    std::string(),
                                    false);
  ok &= Check(!gSystem->AccessPathName(output_directory.c_str(), kFileExists),
              "nested output directory is created automatically");
  ok &= Check(summary.input_files.size() == 1 &&
                  summary.tree_entries == 255 &&
                  summary.channels.size() == 15,
              "synthetic input and output counts");
  for (unsigned int channel = 0; channel < 15; ++channel) {
    const cshine_gamma::GainChannelFitSummary& fit =
        summary.channels[channel];
    ok &= Check(fit.channel == channel && fit.selected_points == 253 &&
                    fit.fit_range_points == 251,
                "strict selection and fit-range point counts");
    ok &= Check(fit.fit_status == 0,
                "ROOT linear fit status for synthetic channel");
    const bool recovered =
        NearlyEqual(fit.intercept, 60.0 + channel, 1.0e-3) &&
        NearlyEqual(fit.slope, 0.1, 1.0e-6);
    if (!recovered) {
      std::cerr << "channel=" << channel << " intercept=" << fit.intercept
                << " intercept_delta="
                << fit.intercept - (60.0 + channel)
                << " slope=" << fit.slope
                << " slope_delta=" << fit.slope - 0.1 << '\n';
    }
    ok &= Check(recovered, "synthetic intercept and slope recovery");
  }

  TFile result(output_path.c_str(), "READ");
  t_2d_fit* parameters = nullptr;
  TCanvas* canvas = nullptr;
  result.GetObject("f_data", parameters);
  result.GetObject("c", canvas);
  ok &= Check(parameters != nullptr && canvas != nullptr,
              "historical ROOT output object names and types");
  if (parameters != nullptr) {
    ok &= Check(std::string(parameters->GetTitle()) == "synthetic_gain.root",
                "configured historical object title");
    ok &= Check(NearlyEqual(parameters->f_2d_fit[4][0][0], 64.0, 1.0e-3) &&
                    NearlyEqual(parameters->f_2d_fit[4][1][0], 0.1, 1.0e-6),
                "f_data stores intercept and slope in historical layout");
  }
  if (canvas != nullptr) {
    TPad* channel_4_pad =
        static_cast<TPad*>(canvas->GetPad(
            cshine_gamma::HistoricalGainCanvasPad(4)));
    TGraph* channel_4_graph = nullptr;
    if (channel_4_pad != nullptr) {
      channel_4_graph = dynamic_cast<TGraph*>(
          channel_4_pad->GetListOfPrimitives()->FindObject("Graph"));
    }
    ok &= Check(channel_4_graph != nullptr &&
                    channel_4_graph->GetN() == 253 &&
                    channel_4_graph->GetFunction("f_4") != nullptr,
                "canvas stores the selected two-dimensional relation and fit");
  }
  ok &= Check(result.GetListOfKeys()->GetEntries() == 2,
              "ROOT output contains f_data and the 4-by-4 canvas");
  result.Close();

  const std::string parameter_text = ReadTextFile(parameter_path);
  ok &= Check(parameter_text.find("double f_2d_fit[][2][2] = {") == 0 &&
                  parameter_text.find("{{60, ") != std::string::npos &&
                  parameter_text.rfind("};\n") ==
                      parameter_text.size() - 3,
              "historical generated parameter-array format");
  const std::string report_text = ReadTextFile(report_path);
  ok &= Check(report_text.find(
                  "config\tlow_gain_selection_exclusive\t150\t600") !=
                  std::string::npos &&
                  report_text.find("channel\t0\t16\t253\t251\t0") !=
                      std::string::npos,
              "run report records selection fit range layout and status");

  bool overwrite_rejected = false;
  try {
    cshine_gamma::FitGainRelation(synthetic,
                                  directory,
                                  output_path,
                                  parameter_path,
                                  report_path,
                                  std::string(),
                                  std::string(),
                                  false);
  } catch (const std::runtime_error&) {
    overwrite_rejected = true;
  }
  ok &= Check(overwrite_rejected, "existing outputs rejected by default");

  gSystem->Unlink(input_path.c_str());
  gSystem->Unlink(output_path.c_str());
  gSystem->Unlink(parameter_path.c_str());
  gSystem->Unlink(report_path.c_str());
  std::remove(output_directory.c_str());
  std::remove((directory + "/nested-results").c_str());
  std::remove(directory.c_str());

  return ok ? 0 : 1;
}
