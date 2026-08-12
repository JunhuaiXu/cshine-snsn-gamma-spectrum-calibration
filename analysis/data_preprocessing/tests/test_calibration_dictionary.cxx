#include "t_2d_fit.h"
#include "t_gamma_cali.h"

#include <TMemFile.h>

#include <cmath>
#include <cstddef>
#include <iostream>
#include <memory>
#include <string>

namespace {

bool NearlyEqual(double left, double right, double tolerance = 1.0e-12) {
  return std::abs(left - right) <= tolerance;
}

bool Check(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAILED: " << message << '\n';
  }
  return condition;
}

void SetCalibrationParameters(t_gamma_cali& calibration) {
  for (std::size_t crystal = 0; crystal < 15; ++crystal) {
    calibration.SetPoint(static_cast<UShort_t>(crystal),
                         0.0,
                         0.0,
                         0.1,
                         0.0);
    calibration.f_gamma_cali_par[crystal][0][0] = 0.0;
    calibration.f_gamma_cali_par[crystal][0][1] = 0.01;
    calibration.f_gamma_cali_par[crystal][1][0] = 0.0;
    calibration.f_gamma_cali_par[crystal][1][1] = 0.0;
  }
}

}  // namespace

int main() {
  bool ok = true;

  t_gamma_cali calibration("calibration", "dictionary test calibration");
  SetCalibrationParameters(calibration);

  ok &= Check(t_2d_fit::Class() != nullptr,
              "ROOT dictionary for t_2d_fit");
  ok &= Check(t_gamma_cali::Class() != nullptr,
              "ROOT dictionary for t_gamma_cali");
  ok &= Check(t_2d_fit::Class()->GetClassVersion() == 2,
              "historical t_2d_fit class version");
  ok &= Check(t_gamma_cali::Class()->GetClassVersion() == 2,
              "historical t_gamma_cali class version");

  {
    TMemFile memory_file("m1_calibration_roundtrip.root", "RECREATE");
    memory_file.WriteObject(&calibration, "cali_test");
    memory_file.Write();

    t_gamma_cali* loaded = nullptr;
    memory_file.GetObject("cali_test", loaded);
    ok &= Check(loaded != nullptr, "ROOT in-memory object round trip");
    if (loaded != nullptr) {
      ok &= Check(NearlyEqual(loaded->GetEnergy(0, 320.0, 3000.0), 30.8),
                  "calibration semantics after ROOT round trip");
    }
  }

  std::unique_ptr<TObject> clone(calibration.Clone("calibration_clone"));
  auto* calibration_clone = dynamic_cast<t_gamma_cali*>(clone.get());
  ok &= Check(calibration_clone != nullptr, "polymorphic calibration clone");
  if (calibration_clone != nullptr) {
    ok &= Check(NearlyEqual(
                    calibration_clone->GetEnergy(0, 320.0, 3000.0), 30.8),
                "calibration semantics after clone");
  }

  return ok ? 0 : 1;
}
