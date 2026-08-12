#include "t_gamma_cali.h"

#include <TFile.h>
#include <TObject.h>

#include <cmath>
#include <cstddef>
#include <iostream>

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0]
              << " CALIBRATION_ROOT_FILE OBJECT_KEY\n";
    return 2;
  }

  TFile input(argv[1], "READ");
  if (input.IsZombie()) {
    std::cerr << "Cannot open ROOT file: " << argv[1] << '\n';
    return 1;
  }

  t_gamma_cali* calibration = nullptr;
  input.GetObject(argv[2], calibration);
  if (calibration == nullptr) {
    TObject* object = input.Get(argv[2]);
    std::cerr << "Cannot read key '" << argv[2]
              << "' as t_gamma_cali";
    if (object != nullptr) {
      std::cerr << "; stored class is " << object->ClassName();
    }
    std::cerr << '\n';
    return 1;
  }

  if (calibration->IsA()->GetClassVersion() != 2) {
    std::cerr << "Unexpected t_gamma_cali class version: "
              << calibration->IsA()->GetClassVersion() << '\n';
    return 1;
  }

  for (std::size_t crystal = 0; crystal < 15; ++crystal) {
    for (std::size_t value_or_error = 0; value_or_error < 2;
         ++value_or_error) {
      for (std::size_t point = 0; point < 3; ++point) {
        if (!std::isfinite(
                calibration->f_gamma_cali_data[crystal][value_or_error]
                                                  [point])) {
          std::cerr << "Non-finite calibration point at crystal " << crystal
                    << '\n';
          return 1;
        }
      }
      for (std::size_t parameter = 0; parameter < 2; ++parameter) {
        if (!std::isfinite(
                calibration->f_gamma_cali_par[crystal][value_or_error]
                                                 [parameter])) {
          std::cerr << "Non-finite calibration parameter at crystal "
                    << crystal << '\n';
          return 1;
        }
      }
    }
  }

  std::cout << "file=" << argv[1] << '\n'
            << "key=" << argv[2] << '\n'
            << "class=" << calibration->ClassName() << '\n'
            << "class_version=" << calibration->IsA()->GetClassVersion()
            << '\n'
            << "name=" << calibration->GetName() << '\n'
            << "title=" << calibration->GetTitle() << '\n';
  return 0;
}
