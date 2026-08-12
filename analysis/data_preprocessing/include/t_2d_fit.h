#ifndef CSHINE_GAMMA_T_2D_FIT_H
#define CSHINE_GAMMA_T_2D_FIT_H

// Provenance: DP-S301, historical t_gamma_cali/t_2d_fit.h.
// The class name, inheritance, stored array layout, and ROOT class version are
// retained so historical calibration objects remain readable.

#include <TNamed.h>
#include <TObject.h>

class t_2d_fit : public TNamed {
 public:
  Double_t f_2d_fit[15][2][2];

  t_2d_fit();
  t_2d_fit(const char* name, const char* title = "");
  t_2d_fit(const t_2d_fit& other);
  t_2d_fit(t_2d_fit&&) = delete;
  ~t_2d_fit() override;

  t_2d_fit& operator=(const t_2d_fit& other);

  void Copy(TObject& object) const override;
  TObject* Clone(const char* new_name = "") const override;
  void Print(Option_t* option = "") const override;

  void copydata(const t_2d_fit& other);

  Double_t GetXE(UShort_t crystal, Double_t low_gain_channel) const {
    if (crystal >= 15) {
      return 0.0;
    }
    return (low_gain_channel - f_2d_fit[crystal][0][0]) /
           f_2d_fit[crystal][1][0];
  }

  Double_t GetE(UShort_t crystal, Double_t high_gain_channel) const {
    if (crystal >= 15) {
      return 0.0;
    }
    return high_gain_channel * f_2d_fit[crystal][1][0] +
           f_2d_fit[crystal][0][0];
  }

  void SetPoint(UShort_t crystal,
                Double_t intercept,
                Double_t intercept_error,
                Double_t slope,
                Double_t slope_error) {
    f_2d_fit[crystal][0][0] = intercept;
    f_2d_fit[crystal][0][1] = intercept_error;
    f_2d_fit[crystal][1][0] = slope;
    f_2d_fit[crystal][1][1] = slope_error;
  }

  ClassDefOverride(t_2d_fit, 2)
};

#endif  // CSHINE_GAMMA_T_2D_FIT_H
