// Provenance: DP-S304, historical t_gamma_cali/t_gamma_cali.C.

#include "t_gamma_cali.h"

#include <TAxis.h>

#include <algorithm>
#include <cstddef>
#include <iostream>

ClassImp(t_gamma_cali)

Double_t t_gamma_cali::GetEnergy_E(UShort_t crystal,
                                    Double_t low_gain_channel) const {
  return GetEnergy_XE(crystal, GetXE(crystal, low_gain_channel));
}

Double_t t_gamma_cali::GetEnergy_XE(UShort_t crystal,
                                     Double_t high_gain_channel) const {
  return high_gain_channel * f_gamma_cali_par[crystal][0][1] +
         f_gamma_cali_par[crystal][0][0];
}

Double_t t_gamma_cali::GetEnergy(UShort_t crystal,
                                  Double_t low_gain_channel,
                                  Double_t high_gain_channel) const {
  if (high_gain_channel > 4000.0) {
    return GetEnergy_E(crystal, low_gain_channel);
  }

  const Double_t converted_high_gain = GetXE(crystal, low_gain_channel);
  if (converted_high_gain < 3000.0) {
    return GetEnergy_XE(crystal, high_gain_channel);
  }
  if (converted_high_gain < 3500.0) {
    const Double_t blended_high_gain =
        (converted_high_gain - 3000.0) / 500.0 * converted_high_gain +
        (3500.0 - converted_high_gain) / 500.0 * high_gain_channel;
    return GetEnergy_XE(crystal, blended_high_gain);
  }
  return GetEnergy_E(crystal, low_gain_channel);
}

UShort_t t_gamma_cali::GetADC_XE(UShort_t crystal, Double_t energy) const {
  return static_cast<UShort_t>(
      (energy - f_gamma_cali_par[crystal][0][0]) /
      f_gamma_cali_par[crystal][0][1]);
}

UShort_t t_gamma_cali::GetADC_E(UShort_t crystal, Double_t energy) const {
  return static_cast<UShort_t>(GetE(crystal, GetADC_XE(crystal, energy)));
}

t_gamma_cali::fit_t t_gamma_cali::SetCaliPoint(
    UShort_t crystal,
    Double_t point_0,
    Double_t point_0_error,
    Double_t point_1,
    Double_t point_1_error,
    Double_t point_2,
    Double_t point_2_error) {
  f_gamma_cali_data[crystal][0][0] = point_0;
  f_gamma_cali_data[crystal][1][0] = point_0_error;
  f_gamma_cali_data[crystal][0][1] = point_1;
  f_gamma_cali_data[crystal][1][1] = point_1_error;
  f_gamma_cali_data[crystal][0][2] = point_2;
  f_gamma_cali_data[crystal][1][2] = point_2_error;
  return Fit(crystal);
}

t_gamma_cali::fit_t t_gamma_cali::Fit(UShort_t crystal) {
  auto graph = std::make_shared<TGraphErrors>(3,
                                               f_gamma_cali_data[crystal][0],
                                               g_gamma_cali_peaks,
                                               f_gamma_cali_data[crystal][1],
                                               g_gamma_cali_peaks_err);
  graph->SetName(Form("g_%d", crystal));
  graph->SetTitle(Form("CH %d Energy-XE", crystal));
  graph->GetXaxis()->SetTitle("XE ADC CH");
  graph->GetYaxis()->SetTitle("Energy (MeV)");
  graph->Print();

  auto fit =
      std::make_shared<TF1>(Form("f_%d", crystal), "pol1", 0.0, 4096.0);
  fit->SetParLimits(0, -200.0, 200.0);
  fit->SetParLimits(1, 0.001, 0.01);
  graph->Fit(fit.get());

  f_gamma_cali_par[crystal][0][0] = fit->GetParameter(0);
  f_gamma_cali_par[crystal][0][1] = fit->GetParameter(1);
  const Double_t* errors = fit->GetParErrors();
  std::copy(errors, errors + 2, f_gamma_cali_par[crystal][1]);
  return std::make_pair(graph, fit);
}

std::array<t_gamma_cali::fit_t, 15> t_gamma_cali::Fit() {
  std::array<fit_t, 15> result;
  for (UShort_t crystal = 0; crystal < 15; ++crystal) {
    result[crystal] = Fit(crystal);
  }
  return result;
}

t_gamma_cali::t_gamma_cali() = default;

t_gamma_cali::t_gamma_cali(const char* name, const char* title)
    : t_2d_fit(name, title) {}

t_gamma_cali::t_gamma_cali(const t_2d_fit& other)
    : t_2d_fit(other), f_gamma_cali_data{}, f_gamma_cali_par{} {}

t_gamma_cali::~t_gamma_cali() = default;

void t_gamma_cali::Copy(TObject& object) const { object = *this; }

t_gamma_cali::t_gamma_cali(const t_gamma_cali& other) : t_2d_fit(other) {
  copydata(other, false);
}

void t_gamma_cali::Set2D(const t_2d_fit* gain_relation) {
  t_2d_fit::copydata(*gain_relation);
}

void t_gamma_cali::copydata(const t_gamma_cali& other, Bool_t copy_parent) {
  if (this == &other) {
    return;
  }
  if (copy_parent) {
    t_2d_fit::copydata(other);
  }
  for (std::size_t crystal = 0; crystal < 15; ++crystal) {
    for (std::size_t value_or_error = 0; value_or_error < 2;
         ++value_or_error) {
      for (std::size_t point = 0; point < 3; ++point) {
        f_gamma_cali_data[crystal][value_or_error][point] =
            other.f_gamma_cali_data[crystal][value_or_error][point];
      }
    }
  }
  for (std::size_t crystal = 0; crystal < 15; ++crystal) {
    for (std::size_t value_or_error = 0; value_or_error < 2;
         ++value_or_error) {
      for (std::size_t parameter = 0; parameter < 2; ++parameter) {
        f_gamma_cali_par[crystal][value_or_error][parameter] =
            other.f_gamma_cali_par[crystal][value_or_error][parameter];
      }
    }
  }
}

t_gamma_cali& t_gamma_cali::operator=(const t_gamma_cali& other) {
  if (this == &other) {
    return *this;
  }
  t_2d_fit::operator=(other);
  copydata(other, false);
  return *this;
}

TObject* t_gamma_cali::Clone(const char* new_name) const {
  auto* clone = static_cast<t_gamma_cali*>(t_2d_fit::Clone(new_name));
  clone->copydata(*this, false);
  return clone;
}

void t_gamma_cali::Print(Option_t*) const {
  t_2d_fit::Print();
  std::cout << "// Gamma Fit Data:" << std::endl;
  std::cout << "Double_t f_gamma_cali_data[15][2][3] = {" << std::endl;
  for (std::size_t crystal = 0; crystal < 15; ++crystal) {
    if (crystal != 0) {
      std::cout << ',' << std::endl;
    }
    std::cout << "\t{";
    for (std::size_t value_or_error = 0; value_or_error < 2;
         ++value_or_error) {
      if (value_or_error != 0) {
        std::cout << ",\t";
      }
      std::cout << '{';
      for (std::size_t point = 0; point < 3; ++point) {
        if (point != 0) {
          std::cout << ",\t";
        }
        std::cout << f_gamma_cali_data[crystal][value_or_error][point];
      }
      std::cout << '}';
    }
    std::cout << '}';
  }
  std::cout << std::endl << "};" << std::endl;
  std::cout << "// Gamma Fit Parameters:" << std::endl;
  std::cout << "Double_t f_gamma_cali_par[15][2][2] = {" << std::endl;
  for (std::size_t crystal = 0; crystal < 15; ++crystal) {
    if (crystal != 0) {
      std::cout << ',' << std::endl;
    }
    std::cout << "\t{";
    for (std::size_t value_or_error = 0; value_or_error < 2;
         ++value_or_error) {
      if (value_or_error != 0) {
        std::cout << ",\t";
      }
      std::cout << '{';
      for (std::size_t parameter = 0; parameter < 2; ++parameter) {
        if (parameter != 0) {
          std::cout << ",\t";
        }
        std::cout << f_gamma_cali_par[crystal][value_or_error][parameter];
      }
      std::cout << '}';
    }
    std::cout << '}';
  }
  std::cout << std::endl << "};" << std::endl;
}
