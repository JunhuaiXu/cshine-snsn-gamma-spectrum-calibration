#ifndef CSHINE_GAMMA_T_GAMMA_CALI_H
#define CSHINE_GAMMA_T_GAMMA_CALI_H

// Provenance: DP-S303, historical t_gamma_cali/t_gamma_cali.h.
// Stored members, public method semantics, and ROOT class version are retained
// for compatibility with historical cali_20240308 objects.

#include "t_2d_fit.h"

#include <TF1.h>
#include <TGraphErrors.h>

#include <array>
#include <memory>
#include <utility>

constexpr Double_t g_gamma_cali_peaks[3] = {1.173, 1.332, 2.614};
constexpr Double_t g_gamma_cali_peaks_err[3] = {};

class t_gamma_cali : public t_2d_fit {
 public:
  Double_t f_gamma_cali_data[15][2][3];
  Double_t f_gamma_cali_par[15][2][2];

  t_gamma_cali();
  t_gamma_cali(const char* name, const char* title = "");
  t_gamma_cali(const t_gamma_cali& other);
  t_gamma_cali(const t_2d_fit& other);
  t_gamma_cali(t_gamma_cali&&) = delete;
  ~t_gamma_cali() override;

  t_gamma_cali& operator=(const t_gamma_cali& other);

  void Copy(TObject& object) const override;
  TObject* Clone(const char* new_name = "") const override;
  void Print(Option_t* option = "") const override;

  void copydata(const t_gamma_cali& other, Bool_t copy_parent = false);
  void Set2D(const t_2d_fit* gain_relation);

  Double_t GetEnergy_E(UShort_t crystal, Double_t low_gain_channel) const;
  Double_t GetEnergy_XE(UShort_t crystal, Double_t high_gain_channel) const;
  Double_t GetEnergy(UShort_t crystal,
                     Double_t low_gain_channel,
                     Double_t high_gain_channel) const;

  template <typename AdcArray>
  std::array<Double_t, 15> GetEnergy(AdcArray& adc) const {
    std::array<Double_t, 15> energies{};
    for (UShort_t crystal = 0; crystal < 15; ++crystal) {
      const UShort_t low_gain_channel = adc[crystal];
      const UShort_t high_gain_channel = adc[crystal + 16];
      energies[crystal] =
          GetEnergy(crystal, low_gain_channel, high_gain_channel);
    }
    return energies;
  }

  UShort_t GetADC_E(UShort_t crystal, Double_t energy) const;
  UShort_t GetADC_XE(UShort_t crystal, Double_t energy) const;

  using fit_t =
      std::pair<std::shared_ptr<TGraphErrors>, std::shared_ptr<TF1>>;

  fit_t SetCaliPoint(UShort_t crystal,
                     Double_t point_0,
                     Double_t point_0_error,
                     Double_t point_1,
                     Double_t point_1_error,
                     Double_t point_2,
                     Double_t point_2_error);

 private:
  fit_t Fit(UShort_t crystal);
  std::array<fit_t, 15> Fit();

 public:
  ClassDefOverride(t_gamma_cali, 2)
};

#endif  // CSHINE_GAMMA_T_GAMMA_CALI_H
