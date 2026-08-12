#include "source_background_spectra.h"

#include <TFile.h>
#include <TH1D.h>
#include <TH1I.h>
#include <TKey.h>
#include <TSystem.h>
#include <TTree.h>

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Event {
  UShort_t energy_0;
  UShort_t time_0;
  UShort_t energy_1;
  UShort_t time_1;
};

bool NearlyEqual(double left, double right, double tolerance = 1.0e-12) {
  return std::abs(left - right) <= tolerance;
}

bool Check(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAILED: " << message << '\n';
  }
  return condition;
}

void WriteSyntheticTree(const std::string& path,
                        const std::vector<Event>& events) {
  TFile output(path.c_str(), "RECREATE");
  if (output.IsZombie()) {
    throw std::runtime_error("cannot create synthetic ROOT file");
  }

  TTree tree("tree", "");
  Event event = {};
  tree.Branch("GAMMA1_HIGH_E", &event.energy_0, "GAMMA1_HIGH_E/s");
  tree.Branch("GAMMA1_T", &event.time_0, "GAMMA1_T/s");
  tree.Branch("GAMMA2_HIGH_E", &event.energy_1, "GAMMA2_HIGH_E/s");
  tree.Branch("GAMMA2_T", &event.time_1, "GAMMA2_T/s");
  for (std::vector<Event>::const_iterator item = events.begin();
       item != events.end(); ++item) {
    event = *item;
    tree.Fill();
  }
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
  bool ok = true;

  const cshine_gamma::SourceBackgroundDefinition central =
      cshine_gamma::Central0308SourceBackgroundDefinition();
  ok &= Check(central.source.first_file == 0 &&
                  central.source.last_file == 20,
              "central source file range");
  ok &= Check(central.background.first_file == 0 &&
                  central.background.last_file == 23,
              "central background file range");
  ok &= Check(NearlyEqual(central.source.live_time_seconds, 2667.0),
              "central source live time");
  ok &= Check(NearlyEqual(central.background.live_time_seconds, 4266.0),
              "central background live time");
  ok &= Check(central.channel_count == 15 && central.adc_bin_count == 4096,
              "central channel and histogram definitions");
  ok &= Check(cshine_gamma::TimeSelection(central, 0) ==
                  "GAMMA1_T>100&&GAMMA1_T<4000",
              "strict historical time selection");
  ok &= Check(cshine_gamma::FormatInputFilename(
                  "/analysis/raw", central.source.prefix, 20) ==
                  "/analysis/raw/a20240308_ThnatCo60.0020.root",
              "historical source filename format");
  ok &= Check(cshine_gamma::BuildInputFileList("/analysis/raw", central.source)
                      .size() == 21 &&
                  cshine_gamma::BuildInputFileList(
                      "/analysis/raw", central.background)
                          .size() == 24,
              "central source and background file counts");
  bool invalid_range_rejected = false;
  try {
    cshine_gamma::RunSampleDefinition invalid_sample = central.source;
    invalid_sample.first_file = 1;
    invalid_sample.last_file = 0;
    cshine_gamma::BuildInputFileList("/analysis/raw", invalid_sample);
  } catch (const std::invalid_argument&) {
    invalid_range_rejected = true;
  }
  ok &= Check(invalid_range_rejected,
              "invalid file range is rejected before allocation");
  ok &= Check(cshine_gamma::SourceHistogramName(4) == "h_src_XE_04" &&
                  cshine_gamma::BackgroundHistogramName(4) ==
                      "h_bkg_XE_04" &&
                  cshine_gamma::NetHistogramName(4) == "h_nobkg_XE_04",
              "historical histogram names");

  std::ostringstream directory_name;
  directory_name << gSystem->TempDirectory()
                 << "/cshine_gamma_source_background_" << gSystem->GetPid();
  const std::string directory = directory_name.str();
  gSystem->mkdir(directory.c_str(), true);

  cshine_gamma::SourceBackgroundDefinition synthetic = central;
  synthetic.name = "synthetic";
  synthetic.source.prefix = "SYNTH_SRC";
  synthetic.source.first_file = 0;
  synthetic.source.last_file = 0;
  synthetic.source.live_time_seconds = 2.0;
  synthetic.background.prefix = "SYNTH_BKG";
  synthetic.background.first_file = 0;
  synthetic.background.last_file = 0;
  synthetic.background.live_time_seconds = 4.0;
  synthetic.channel_count = 2;

  const std::string source_path = cshine_gamma::FormatInputFilename(
      directory, synthetic.source.prefix, 0);
  const std::string background_path = cshine_gamma::FormatInputFilename(
      directory, synthetic.background.prefix, 0);
  const std::string output_directory = directory + "/nested-results/m2";
  const std::string output_path = output_directory + "/net.root";
  const std::string report_path = output_directory + "/net.run.tsv";

  const std::vector<Event> source_events = {
      {100, 101, 110, 101},
      {200, 3999, 210, 3999},
      {300, 100, 310, 100},
      {400, 4000, 410, 4000},
      {500, 50, 510, 50},
  };
  const std::vector<Event> background_events = {
      {100, 101, 110, 101},
      {300, 3999, 310, 3999},
      {400, 100, 410, 100},
      {500, 4000, 510, 4000},
  };
  WriteSyntheticTree(source_path, source_events);
  WriteSyntheticTree(background_path, background_events);

  const cshine_gamma::SourceBackgroundSummary summary =
      cshine_gamma::BuildSourceBackgroundSpectra(
          synthetic, directory, output_path, report_path, false);
  ok &= Check(!gSystem->AccessPathName(output_directory.c_str(), kFileExists),
              "nested output directory is created automatically");
  ok &= Check(summary.source_tree_entries == 5 &&
                  summary.background_tree_entries == 4,
              "synthetic input tree entries");
  ok &= Check(summary.channels.size() == 2 &&
                  summary.channels[0].source_selected_entries == 2 &&
                  summary.channels[0].background_selected_entries == 2,
              "strict time-boundary event selection");

  TFile result(output_path.c_str(), "READ");
  TH1I* source_histogram = nullptr;
  TH1I* background_histogram = nullptr;
  TH1D* net_histogram = nullptr;
  result.GetObject("h_src_XE_00", source_histogram);
  result.GetObject("h_bkg_XE_00", background_histogram);
  result.GetObject("h_nobkg_XE_00", net_histogram);
  ok &= Check(source_histogram != nullptr && background_histogram != nullptr &&
                  net_histogram != nullptr,
              "historical output object names and types");
  if (source_histogram != nullptr && background_histogram != nullptr &&
      net_histogram != nullptr) {
    ok &= Check(source_histogram->GetNbinsX() == 4096 &&
                    NearlyEqual(source_histogram->GetXaxis()->GetXmin(), 0.0) &&
                    NearlyEqual(source_histogram->GetXaxis()->GetXmax(),
                                4096.0),
                "historical ADC histogram binning");
    ok &= Check(std::string(source_histogram->GetTitle()) ==
                    "Source Gamma_XE[00]" &&
                    std::string(background_histogram->GetTitle()) ==
                        "Bkg Gamma_XE[00]" &&
                    std::string(net_histogram->GetTitle()).empty(),
                "historical histogram titles");
    ok &= Check(source_histogram->GetSumw2N() == 0 &&
                    background_histogram->GetSumw2N() == 0 &&
                    net_histogram->GetSumw2N() > 0,
                "historical raw and propagated-error storage");
    ok &= Check(NearlyEqual(source_histogram->GetBinContent(101), 1.0) &&
                    NearlyEqual(source_histogram->GetBinContent(201), 1.0) &&
                    NearlyEqual(source_histogram->GetBinContent(301), 0.0),
                "source spectrum after strict time selection");
    ok &= Check(NearlyEqual(background_histogram->GetBinContent(101), 1.0) &&
                    NearlyEqual(background_histogram->GetBinContent(301),
                                1.0),
                "background spectrum after strict time selection");
    ok &= Check(NearlyEqual(net_histogram->GetBinContent(101), 0.25) &&
                    NearlyEqual(net_histogram->GetBinContent(201), 0.5) &&
                    NearlyEqual(net_histogram->GetBinContent(301), -0.25),
                "live-time-normalized source-minus-background spectrum");
    ok &= Check(NearlyEqual(net_histogram->GetBinError(101),
                            std::sqrt(0.25 + 0.0625)),
                "statistical-error propagation through scaled subtraction");
  }
  ok &= Check(result.GetListOfKeys()->GetEntries() == 6,
              "three output histograms for each configured channel");
  result.Close();

  const std::string report = ReadTextFile(report_path);
  ok &= Check(report.find("config\tsource_live_time_seconds\t2") !=
                  std::string::npos &&
                  report.find("channel\t0\t2\t2") != std::string::npos,
              "run report records normalization and selected entries");

  gSystem->Unlink(source_path.c_str());
  gSystem->Unlink(background_path.c_str());
  gSystem->Unlink(output_path.c_str());
  gSystem->Unlink(report_path.c_str());
  std::remove(output_directory.c_str());
  std::remove((directory + "/nested-results").c_str());
  std::remove(directory.c_str());

  return ok ? 0 : 1;
}
