// Provenance: DP-S302, historical t_gamma_cali/t_2d_fit.C.

#include "t_2d_fit.h"

#include <cstddef>
#include <iostream>

ClassImp(t_2d_fit)

t_2d_fit::t_2d_fit() = default;

t_2d_fit::t_2d_fit(const char* name, const char* title)
    : TNamed(name, title) {}

t_2d_fit::~t_2d_fit() = default;

t_2d_fit::t_2d_fit(const t_2d_fit& other) : TNamed(other) {
  copydata(other);
}

void t_2d_fit::copydata(const t_2d_fit& other) {
  if (this == &other) {
    return;
  }
  for (std::size_t crystal = 0; crystal < 15; ++crystal) {
    for (std::size_t value_or_error = 0; value_or_error < 2;
         ++value_or_error) {
      for (std::size_t parameter = 0; parameter < 2; ++parameter) {
        f_2d_fit[crystal][value_or_error][parameter] =
            other.f_2d_fit[crystal][value_or_error][parameter];
      }
    }
  }
}

t_2d_fit& t_2d_fit::operator=(const t_2d_fit& other) {
  if (this == &other) {
    return *this;
  }
  TNamed::operator=(other);
  copydata(other);
  return *this;
}

void t_2d_fit::Copy(TObject& object) const { object = *this; }

TObject* t_2d_fit::Clone(const char* new_name) const {
  auto* clone = static_cast<t_2d_fit*>(TNamed::Clone(new_name));
  clone->copydata(*this);
  return clone;
}

void t_2d_fit::Print(Option_t*) const {
  std::cout << "Name: " << GetName() << "\t Title: " << GetTitle()
            << std::endl;
  std::cout << "// 2D Fit Data:" << std::endl;
  std::cout << "Double_t f_2d_fit[15][2][2] = {" << std::endl;
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
        std::cout << f_2d_fit[crystal][value_or_error][parameter];
      }
      std::cout << '}';
    }
    std::cout << '}';
  }
  std::cout << std::endl << "};" << std::endl;
}
