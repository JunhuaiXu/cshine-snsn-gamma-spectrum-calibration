#include "jiugong_recon.h"
#include "shower_reconstruction.h"

#include <TClass.h>
#include <TFile.h>
#include <TSystem.h>
#include <TTree.h>
#include <TTreeReader.h>
#include <TTreeReaderValue.h>

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <sys/types.h>
#include <unistd.h>

namespace {

bool Check(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAILED: " << message << '\n';
  }
  return condition;
}

bool NearlyEqual(double left, double right, double tolerance = 1.0e-12) {
  return std::abs(left - right) <= tolerance;
}

std::array<double, 15> ZeroEnergy() {
  std::array<double, 15> value = {};
  value.fill(0.0);
  return value;
}

std::array<double, 15> NaNTime() {
  std::array<double, 15> value;
  value.fill(std::numeric_limits<double>::quiet_NaN());
  return value;
}

}  // namespace

int main() {
  bool ok = true;
  const cshine_gamma::ShowerReconstructionDefinition definition =
      cshine_gamma::HistoricalShowerReconstructionDefinition();
  ok &= Check(definition.crystal_count == 15U, "15-crystal definition");
  ok &= Check(definition.crystal_energy_threshold_mev == 1.0,
              "one-MeV threshold");
  ok &= Check(definition.neighbor_time_window_ns == 50.0,
              "50-ns neighbor window");
  ok &= Check(definition.separate_core_time_ns == 100.0,
              "100-ns separate-core window");
  ok &= Check(cshine_gamma::ClassifyCrystal(5U) ==
                  cshine_gamma::CrystalRole::kCentral,
              "central crystal classification");
  ok &= Check(cshine_gamma::ClassifyCrystal(7U) ==
                  cshine_gamma::CrystalRole::kMainSide,
              "main side classification");
  ok &= Check(cshine_gamma::ClassifyCrystal(1U) ==
                  cshine_gamma::CrystalRole::kLowerEdgeWithoutVeto,
              "lower edge classification");
  ok &= Check(cshine_gamma::ClassifyCrystal(0U) ==
                  cshine_gamma::CrystalRole::kCorner,
              "corner classification");

  std::array<double, 15> energy = ZeroEnergy();
  std::array<double, 15> time = NaNTime();
  energy[5] = 10.0;
  time[5] = 0.0;
  energy[6] = 2.0;
  time[6] = 50.0;
  energy[10] = 3.0;
  time[10] = -50.0;
  energy[12] = 0.999;
  time[12] = 0.0;
  std::map<unsigned short, jiugong_recon_result_t> reconstructed =
      jiugong_recon(energy, time);
  ok &= Check(reconstructed.size() == 1U, "neighboring deposits form one core");
  ok &= Check(reconstructed.count(5U) == 1U, "highest-energy center retained");
  ok &= Check(reconstructed[5U].GetMultiplicity() == 3U,
              "orthogonal and diagonal deposits merged at 50 ns");
  ok &= Check(NearlyEqual(reconstructed[5U].GetEnergy(), 15.0),
              "cluster total energy");
  ok &= Check(NearlyEqual(reconstructed[5U].GetJoinedCrystals().at(5U), 10.0),
              "core energy retained separately");

  energy = ZeroEnergy();
  time = NaNTime();
  energy[5] = 10.0;
  time[5] = 0.0;
  energy[6] = 2.0;
  time[6] = 50.001;
  reconstructed = jiugong_recon(energy, time);
  ok &= Check(reconstructed.size() == 2U,
              "within-100-ns nonmerged signal preserves placeholder entry");
  ok &= Check(reconstructed[6U].GetCenter() == -1 &&
                  reconstructed[6U].GetMultiplicity() == 0U,
              "historical invalid-center placeholder");

  energy[6] = 2.0;
  time[6] = 100.001;
  reconstructed = jiugong_recon(energy, time);
  ok &= Check(reconstructed.size() == 2U &&
                  reconstructed[6U].GetCenter() == 6,
              "time separation greater than 100 ns forms second core");

  energy = ZeroEnergy();
  time = NaNTime();
  energy[5] = 0.999;
  time[5] = 0.0;
  ok &= Check(jiugong_recon(energy, time).empty(),
              "maximum below one MeV rejected");
  energy[5] = 1.0;
  ok &= Check(!jiugong_recon(energy, time).empty(),
              "one-MeV boundary included");

  jiugong_recon_result_t central(5U, 10.0);
  jiugong_recon_result_t side(7U, 10.0);
  jiugong_recon_result_t lower_edge(1U, 10.0);
  jiugong_recon_result_t placeholder;
  ok &= Check(cshine_gamma::IsMainSpectrumCandidate(central, 3U),
              "central candidate does not require veto silence");
  ok &= Check(cshine_gamma::IsMainSpectrumCandidate(side, 0U),
              "side candidate accepted with all veto faces silent");
  ok &= Check(!cshine_gamma::IsMainSpectrumCandidate(side, 1U),
              "side candidate rejected when any veto face fires");
  ok &= Check(!cshine_gamma::IsMainSpectrumCandidate(lower_edge, 0U),
              "lower edge excluded from main core set");
  ok &= Check(!cshine_gamma::IsMainSpectrumCandidate(placeholder, 0U),
              "placeholder is not a physical candidate");

  ok &= Check(cshine_gamma::CountVetoSignals({{100, 101, 4000}}) == 1U,
              "strict veto TDC boundaries");
  ok &= Check(cshine_gamma::CountVetoSignals({{101, 3999, 2000}}) == 3U,
              "all three veto signals counted");

  TClass* result_class = TClass::GetClass("jiugong_recon_result_t");
  TClass* map_class =
      TClass::GetClass("map<unsigned short,jiugong_recon_result_t>");
  ok &= Check(result_class != nullptr, "reconstruction result dictionary");
  ok &= Check(map_class != nullptr, "reconstruction map dictionary");

  const std::string path =
      "/tmp/cshine_shower_reconstruction_test_" +
      std::to_string(static_cast<long long>(getpid())) + ".root";
  {
    TFile output(path.c_str(), "RECREATE");
    TTree tree("GammaCaliData", "streaming test");
    std::map<unsigned short, jiugong_recon_result_t> record;
    tree.Branch("recon_result", &record);
    record[5U] = central;
    record[7U] = side;
    tree.Fill();
    tree.Write();
  }
  {
    TFile input(path.c_str(), "READ");
    TTree* tree = nullptr;
    input.GetObject("GammaCaliData", tree);
    ok &= Check(tree != nullptr, "streamed reconstruction tree");
    if (tree != nullptr) {
      TTreeReader reader(tree);
      TTreeReaderValue<std::map<unsigned short, jiugong_recon_result_t> > value(
          reader, "recon_result");
      ok &= Check(reader.Next(), "read streamed reconstruction entry");
      ok &= Check(value->size() == 2U && value->at(5U).GetCenter() == 5,
                  "streamed reconstruction contents");
    }
  }
  gSystem->Unlink(path.c_str());
  return ok ? 0 : 1;
}
