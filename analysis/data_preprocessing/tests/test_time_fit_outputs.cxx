#include "time_fit_outputs.h"

#include <TCanvas.h>
#include <TFile.h>
#include <TF1.h>
#include <TH2D.h>
#include <TSystem.h>

#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

bool Check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAILED: " << message << '\n';
  }
  return condition;
}

std::string BaseName(unsigned int crystal) {
  std::ostringstream name;
  name << "f_" << std::setfill('0') << std::setw(2) << crystal;
  return name.str();
}

}  // namespace

int main() {
  bool ok = true;
  std::ostringstream directory_name;
  directory_name << gSystem->TempDirectory()
                 << "/cshine_time_fit_output_test_" << gSystem->GetPid();
  const std::string directory = directory_name.str();
  gSystem->mkdir(directory.c_str(), true);

  const std::array<cshine_gamma::TimeWalkParameter, 15>& parameters =
      cshine_gamma::CentralTimeWalkParameters();
  for (unsigned int crystal = 0; crystal < parameters.size(); ++crystal) {
    const std::string base = directory + "/" + BaseName(crystal);
    {
      TFile output((base + ".root").c_str(), "RECREATE");
      TCanvas canvas("Canvas_1", "Canvas_1", 400, 300);
      TH2D histogram("h_time_adc", "", 8, 0.0, 8.0, 8, 0.0, 8.0);
      histogram.Fill(2.0, 3.0);
      histogram.GetListOfFunctions()->Add(
          new TF1("f_inv", "[0]+[1]/([2]-x)", 0.0, 8.0));
      histogram.Draw("COLZ");
      canvas.Write();
    }
    {
      std::ofstream text((base + ".out").c_str());
      text << "****************************************\n"
           << "Minimizer is Minuit / Migrad\n"
           << "Chi2 = 10\nNDf = 2\nEdm = 1e-9\nNCalls = 100\n"
           << std::setprecision(12)
           << "C0 = " << parameters[crystal].c0_ns_channel
           << " +/- 1 (limited)\n"
           << "E0 = " << parameters[crystal].e0_channel
           << " +/- 2 (limited)\n"
           << "T0 = " << parameters[crystal].t0_ns
           << " +/- 3 (limited)\n";
    }
  }

  const std::vector<cshine_gamma::TimeFitArtifactSummary> summaries =
      cshine_gamma::InspectTimeFitArtifacts(directory, 1.0e-9);
  ok &= Check(summaries.size() == 15, "all 15 artifact pairs are inspected");
  ok &= Check(summaries[0].root_file == "f_00.root" &&
                  summaries[14].text_file == "f_14.out",
              "stable channel filenames");
  ok &= Check(summaries[5].histogram_name == "h_time_adc" &&
                  summaries[5].fit_function_name == "f_inv",
              "Canvas_1 histogram and attached fit are recorded");
  ok &= Check(summaries[0].parameters_match_production &&
                  summaries[14].parameters_match_production,
              "historical text values match production parameters");

  const std::string report = directory + "/nested/report.tsv";
  cshine_gamma::WriteTimeFitArtifactReport(report, summaries, false);
  std::ifstream report_stream(report.c_str());
  std::stringstream report_text;
  report_text << report_stream.rdbuf();
  ok &= Check(report_text.str().find("parameters_match_production") !=
                      std::string::npos &&
                  report_text.str().find("f_14.root") != std::string::npos,
              "audit report records schema and final channel");

  bool overwrite_rejected = false;
  try {
    cshine_gamma::WriteTimeFitArtifactReport(report, summaries, false);
  } catch (const std::runtime_error&) {
    overwrite_rejected = true;
  }
  ok &= Check(overwrite_rejected, "existing audit report is protected");

  for (unsigned int crystal = 0; crystal < parameters.size(); ++crystal) {
    const std::string base = directory + "/" + BaseName(crystal);
    gSystem->Unlink((base + ".root").c_str());
    gSystem->Unlink((base + ".out").c_str());
  }
  gSystem->Unlink(report.c_str());
  std::remove((directory + "/nested").c_str());
  std::remove(directory.c_str());

  return ok ? 0 : 1;
}
