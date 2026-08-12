#include "calibrated_event_tree.h"

#include "t_gamma_cali.h"
#include "time_calibration.h"

#include <TFile.h>
#include <TSystem.h>
#include <TTree.h>
#include <TTreeReader.h>
#include <TTreeReaderArray.h>

#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

namespace {

struct Event {
  std::array<Int_t, 15> low_gain;
  std::array<Int_t, 15> high_gain;
  std::array<Int_t, 15> gamma_tdc;
  std::array<Int_t, 4> t0_tdc;
  std::array<Int_t, 3> veto_adc;
  std::array<Int_t, 3> veto_tdc;
  std::array<UShort_t, 32> trigger_tdc;
};

bool Check(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAILED: " << message << '\n';
  }
  return condition;
}

bool NearlyEqual(double left, double right, double tolerance = 1.0e-11) {
  return std::abs(left - right) <= tolerance;
}

std::string TestDirectory() {
  return "/tmp/cshine_calibrated_event_tree_test_" +
         std::to_string(static_cast<long long>(getpid()));
}

void SetCalibrationParameters(t_gamma_cali& calibration) {
  for (std::size_t crystal = 0; crystal < 15; ++crystal) {
    calibration.SetPoint(static_cast<UShort_t>(crystal), 0.0, 0.0, 0.1, 0.0);
    calibration.f_gamma_cali_par[crystal][0][0] = 0.0;
    calibration.f_gamma_cali_par[crystal][0][1] = 0.01;
    calibration.f_gamma_cali_par[crystal][1][0] = 0.0;
    calibration.f_gamma_cali_par[crystal][1][1] = 0.0;
  }
}

void WriteCalibration(const std::string& path) {
  TFile output(path.c_str(), "RECREATE");
  t_gamma_cali calibration("cali_20240308", "synthetic calibration");
  SetCalibrationParameters(calibration);
  output.WriteObject(&calibration, "cali_20240308");
  output.Close();
}

void WriteRawTree(const std::string& path,
                  const std::vector<Event>& events,
                  bool omit_last_high_gain = false) {
  TFile output(path.c_str(), "RECREATE");
  TTree tree("tree", "synthetic raw detector events");
  std::array<Int_t, 15> low_gain = {};
  std::array<Int_t, 15> high_gain = {};
  std::array<Int_t, 15> gamma_tdc = {};
  std::array<Int_t, 4> t0_tdc = {};
  std::array<Int_t, 3> veto_adc = {};
  std::array<Int_t, 3> veto_tdc = {};
  std::array<UShort_t, 32> trigger_tdc = {};
  std::array<std::string, 15> low_names;
  std::array<std::string, 15> high_names;
  std::array<std::string, 15> time_names;
  std::array<std::string, 4> t0_names;
  std::array<std::string, 3> veto_adc_names;
  std::array<std::string, 3> veto_tdc_names;
  for (unsigned int crystal = 0; crystal < 15; ++crystal) {
    low_names[crystal] = "GAMMA" + std::to_string(crystal + 1U) + "_LOW_E";
    high_names[crystal] =
        "GAMMA" + std::to_string(crystal + 1U) + "_HIGH_E";
    time_names[crystal] = "GAMMA" + std::to_string(crystal + 1U) + "_T";
    tree.Branch(low_names[crystal].c_str(), &low_gain[crystal],
                (low_names[crystal] + "/I").c_str());
    if (!(omit_last_high_gain && crystal == 14U)) {
      tree.Branch(high_names[crystal].c_str(), &high_gain[crystal],
                  (high_names[crystal] + "/I").c_str());
    }
    tree.Branch(time_names[crystal].c_str(), &gamma_tdc[crystal],
                (time_names[crystal] + "/I").c_str());
  }
  for (unsigned int t0 = 0; t0 < 4; ++t0) {
    t0_names[t0] = "T0" + std::to_string(t0 + 1U) + "_T";
    tree.Branch(t0_names[t0].c_str(), &t0_tdc[t0],
                (t0_names[t0] + "/I").c_str());
  }
  for (unsigned int veto = 0; veto < 3; ++veto) {
    veto_adc_names[veto] = "Veto" + std::to_string(veto + 1U) + "_E";
    veto_tdc_names[veto] = "Veto" + std::to_string(veto + 1U) + "_T";
    tree.Branch(veto_adc_names[veto].c_str(), &veto_adc[veto],
                (veto_adc_names[veto] + "/I").c_str());
    tree.Branch(veto_tdc_names[veto].c_str(), &veto_tdc[veto],
                (veto_tdc_names[veto] + "/I").c_str());
  }
  tree.Branch("TDC_Gamma_Trig", &trigger_tdc);
  for (std::vector<Event>::const_iterator event = events.begin();
       event != events.end(); ++event) {
    low_gain = event->low_gain;
    high_gain = event->high_gain;
    gamma_tdc = event->gamma_tdc;
    t0_tdc = event->t0_tdc;
    veto_adc = event->veto_adc;
    veto_tdc = event->veto_tdc;
    trigger_tdc = event->trigger_tdc;
    tree.Fill();
  }
  tree.Write();
  output.Close();
}

Event MakeEvent(Int_t low_gain, Int_t high_gain, Int_t gamma_tdc) {
  Event event = {};
  event.low_gain.fill(low_gain);
  event.high_gain.fill(high_gain);
  event.gamma_tdc.fill(gamma_tdc);
  event.t0_tdc = {{101, 200, 300, 4000}};
  event.veto_adc = {{11, 22, 33}};
  event.veto_tdc = {{100, 101, 4000}};
  for (unsigned int channel = 0; channel < 32U; ++channel) {
    event.trigger_tdc[channel] = static_cast<UShort_t>(100U + channel);
  }
  return event;
}

}  // namespace

int main() {
  bool ok = true;
  const std::string directory = TestDirectory();
  gSystem->mkdir(directory.c_str(), true);
  const std::string calibration_file = directory + "/calibration.root";
  const std::string input_file = directory + "/raw_run.000.part0.root";
  const std::string output_file = directory + "/nested/calibrated.root";
  const std::string report_file = directory + "/nested/calibrated.tsv";
  const std::string missing_file = directory + "/missing_branch.root";
  const std::string missing_output = directory + "/missing_output.root";
  WriteCalibration(calibration_file);
  const std::vector<Event> events = {
      MakeEvent(100, 900, 101),
      MakeEvent(320, 3000, 100),
      MakeEvent(360, 3000, 4000),
      MakeEvent(100, 4001, 3999),
  };
  WriteRawTree(input_file, events);
  WriteRawTree(missing_file, events, true);

  const cshine_gamma::CalibratedEventDefinition definition =
      cshine_gamma::Central0308CalibratedEventDefinition();
  ok &= Check(definition.input_tree_name == "tree", "input tree name");
  ok &= Check(definition.output_tree_name == "GammaCaliData",
              "output tree name");
  ok &= Check(definition.calibration_object_name == "cali_20240308",
              "calibration object name");

  const cshine_gamma::CalibratedEventSummary summary =
      cshine_gamma::BuildCalibratedEventTree(
          definition, std::vector<std::string>{directory + "/raw_run.000*.root"},
          calibration_file, output_file, report_file, false);
  ok &= Check(summary.resolved_input_files.size() == 1U,
              "resolved input file count");
  ok &= Check(summary.input_entries == events.size(), "input entry count");
  ok &= Check(summary.output_entries == events.size(), "output entry count");
  ok &= Check(summary.channels[0].invalid_time_count == 2U,
              "strict time validity boundaries");
  ok &= Check(summary.channels[0].high_gain_count == 1U,
              "high-gain energy branch count");
  ok &= Check(summary.channels[0].blended_gain_count == 1U,
              "blended energy branch count");
  ok &= Check(summary.channels[0].low_gain_count == 2U,
              "low-gain energy branch count");
  ok &= Check(summary.channels[0].saturated_high_gain_count == 1U,
              "saturated high-gain branch count");

  TFile output(output_file.c_str(), "READ");
  TTree* tree = nullptr;
  output.GetObject("GammaCaliData", tree);
  ok &= Check(tree != nullptr, "output GammaCaliData tree");
  if (tree != nullptr) {
    ok &= Check(tree->GetEntries() == static_cast<Long64_t>(events.size()),
                "output tree entries");
    ok &= Check(tree->GetBranch("GammaEnergy") != nullptr,
                "GammaEnergy branch");
    ok &= Check(tree->GetBranch("GammaTime") != nullptr, "GammaTime branch");
    ok &= Check(tree->GetBranch("ADC_Gamma") != nullptr, "ADC_Gamma branch");
    ok &= Check(tree->GetBranch("TDC_Gamma") != nullptr, "TDC_Gamma branch");
    ok &= Check(tree->GetBranch("TDC_Gamma_Trig_list") != nullptr,
                "TDC_Gamma_Trig_list branch");
    ok &= Check(tree->GetBranch("TDC_T0") != nullptr, "TDC_T0 branch");
    ok &= Check(tree->GetBranch("ADC_Veto") != nullptr, "ADC_Veto branch");
    ok &= Check(tree->GetBranch("TDC_Veto") != nullptr, "TDC_Veto branch");

    TTreeReader reader(tree);
    TTreeReaderArray<Double_t> energy(reader, "GammaEnergy");
    TTreeReaderArray<Double_t> time(reader, "GammaTime");
    TTreeReaderArray<UShort_t> adc(reader, "ADC_Gamma");
    TTreeReaderArray<UShort_t> gamma_tdc(reader, "TDC_Gamma");
    TTreeReaderArray<UShort_t> trigger_tdc(reader,
                                          "TDC_Gamma_Trig_list");
    TTreeReaderArray<UShort_t> t0(reader, "TDC_T0");
    TTreeReaderArray<Int_t> veto_adc(reader, "ADC_Veto");
    TTreeReaderArray<Int_t> veto_tdc(reader, "TDC_Veto");
    unsigned int entry = 0;
    const std::array<double, 4> expected_energy = {{9.0, 30.8, 36.0, 10.0}};
    while (reader.Next()) {
      ok &= Check(energy.GetSize() == 15U, "GammaEnergy array size");
      ok &= Check(time.GetSize() == 15U, "GammaTime array size");
      ok &= Check(adc.GetSize() == 32U, "ADC_Gamma array size");
      ok &= Check(gamma_tdc.GetSize() == 32U, "TDC_Gamma array size");
      ok &= Check(trigger_tdc.GetSize() == 32U,
                  "TDC_Gamma_Trig_list array size");
      ok &= Check(t0.GetSize() == 4U, "TDC_T0 array size");
      ok &= Check(veto_adc.GetSize() == 3U, "ADC_Veto array size");
      ok &= Check(veto_tdc.GetSize() == 3U, "TDC_Veto array size");
      ok &= Check(NearlyEqual(energy[0], expected_energy[entry]),
                  "historical energy path result");
      const double expected_time = cshine_gamma::CorrectedGammaTimeOrNaNNs(
          0U, static_cast<UShort_t>(events[entry].gamma_tdc[0]),
          static_cast<UShort_t>(events[entry].low_gain[0]));
      if (std::isnan(expected_time)) {
        ok &= Check(std::isnan(time[0]), "invalid time stored as NaN");
      } else {
        ok &= Check(NearlyEqual(time[0], expected_time),
                    "production corrected time result");
      }
      ok &= Check(adc[0] == static_cast<UShort_t>(events[entry].low_gain[0]),
                  "retained low-gain ADC");
      ok &= Check(adc[16] == static_cast<UShort_t>(events[entry].high_gain[0]),
                  "retained high-gain ADC");
      ok &= Check(adc[15] == 0U && adc[31] == 0U,
                  "historical unused ADC slots");
      ok &= Check(gamma_tdc[15] == 0U && gamma_tdc[31] == 0U,
                  "historical unused gamma-TDC slots");
      ok &= Check(trigger_tdc[17] == events[entry].trigger_tdc[17],
                  "hardware trigger-monitor TDC retained");
      ok &= Check(t0[3] == 4000U, "T0 retained without time calculation");
      ok &= Check(veto_adc[1] == 22 && veto_tdc[1] == 101,
                  "veto channels retained without reinterpretation");
      ++entry;
    }
    ok &= Check(entry == events.size(), "read all output entries");
  }
  output.Close();

  std::ifstream report(report_file.c_str());
  std::string report_text((std::istreambuf_iterator<char>(report)),
                          std::istreambuf_iterator<char>());
  ok &= Check(report_text.find("input_file\t0\t") != std::string::npos,
              "report records resolved input file");
  ok &= Check(report_text.find("summary\toutput_entries\t4") !=
                  std::string::npos,
              "report output entry count");

  bool overwrite_rejected = false;
  try {
    cshine_gamma::BuildCalibratedEventTree(
        definition, std::vector<std::string>{input_file}, calibration_file,
        output_file, report_file, false);
  } catch (const std::runtime_error&) {
    overwrite_rejected = true;
  }
  ok &= Check(overwrite_rejected, "existing output rejection");

  bool missing_branch_rejected = false;
  try {
    cshine_gamma::BuildCalibratedEventTree(
        definition, std::vector<std::string>{missing_file}, calibration_file,
        missing_output, directory + "/missing_output.tsv", false);
  } catch (const std::runtime_error&) {
    missing_branch_rejected = true;
  }
  ok &= Check(missing_branch_rejected, "missing branch rejection");

  return ok ? 0 : 1;
}
