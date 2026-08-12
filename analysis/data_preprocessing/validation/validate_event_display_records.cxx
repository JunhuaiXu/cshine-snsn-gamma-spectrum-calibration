#include <TCanvas.h>
#include <TClass.h>
#include <TFile.h>
#include <TH2.h>
#include <TKey.h>
#include <TList.h>
#include <TObject.h>
#include <TPad.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

TH2* find_histogram_in_pad(TVirtualPad& pad) {
  TIter next(pad.GetListOfPrimitives());
  while (TObject* object = next()) {
    if (object->InheritsFrom(TH2::Class())) {
      return static_cast<TH2*>(object);
    }
    if (object->InheritsFrom(TVirtualPad::Class())) {
      TH2* nested = find_histogram_in_pad(*static_cast<TVirtualPad*>(object));
      if (nested) {
        return nested;
      }
    }
  }
  return nullptr;
}

TH2* require_historical_histogram(TFile& input) {
  TIter next_key(input.GetListOfKeys());
  while (TKey* key = static_cast<TKey*>(next_key())) {
    TObject* object = key->ReadObj();
    if (object->InheritsFrom(TH2::Class())) {
      return static_cast<TH2*>(object);
    }
    if (object->InheritsFrom(TVirtualPad::Class())) {
      TH2* histogram =
          find_histogram_in_pad(*static_cast<TVirtualPad*>(object));
      if (histogram) {
        return histogram;
      }
    }
  }
  throw std::runtime_error("No TH2 histogram found in historical ROOT file.");
}

TH2* require_histogram(TFile& input, const char* name) {
  TH2* histogram = nullptr;
  input.GetObject(name, histogram);
  if (!histogram) {
    throw std::runtime_error(std::string("Missing output histogram: ") + name);
  }
  return histogram;
}

long long compare_histograms(const TH2& migrated, const TH2& historical,
                             double tolerance, double& maximum_difference) {
  if (migrated.GetNbinsX() != historical.GetNbinsX() ||
      migrated.GetNbinsY() != historical.GetNbinsY()) {
    throw std::runtime_error("Histogram dimensions differ.");
  }
  long long mismatch_count = 0;
  maximum_difference = 0.0;
  for (int x = 0; x <= migrated.GetNbinsX() + 1; ++x) {
    for (int y = 0; y <= migrated.GetNbinsY() + 1; ++y) {
      const double difference = std::abs(migrated.GetBinContent(x, y) -
                                         historical.GetBinContent(x, y));
      maximum_difference = std::max(maximum_difference, difference);
      if (difference > tolerance) {
        ++mismatch_count;
      }
    }
  }
  return mismatch_count;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 5) {
    std::cerr << "Usage: " << argv[0]
              << " MIGRATED_ROOT HISTORICAL_GAMMA_ROOT "
                 "HISTORICAL_COSMIC_ROOT REPORT_TSV"
              << std::endl;
    return 2;
  }
  try {
    TFile migrated_file(argv[1], "READ");
    TFile gamma_file(argv[2], "READ");
    TFile cosmic_file(argv[3], "READ");
    if (migrated_file.IsZombie() || gamma_file.IsZombie() ||
        cosmic_file.IsZombie()) {
      throw std::runtime_error("At least one validation ROOT file cannot be opened.");
    }
    TH2* migrated_gamma =
        require_histogram(migrated_file, "gamma_event_display");
    TH2* migrated_cosmic =
        require_histogram(migrated_file, "cosmic_event_display");
    TH2* historical_gamma = require_historical_histogram(gamma_file);
    TH2* historical_cosmic = require_historical_histogram(cosmic_file);

    double gamma_maximum_difference = 0.0;
    double cosmic_maximum_difference = 0.0;
    const long long gamma_mismatches = compare_histograms(
        *migrated_gamma, *historical_gamma, 1.0e-12, gamma_maximum_difference);
    const long long cosmic_mismatches = compare_histograms(
        *migrated_cosmic, *historical_cosmic, 1.0e-12,
        cosmic_maximum_difference);

    std::ofstream report(argv[4]);
    if (!report) {
      throw std::runtime_error("Cannot create validation report.");
    }
    report << "panel\tmismatched_bins\tmaximum_absolute_difference\tstatus\n";
    report << "gamma\t" << gamma_mismatches << '\t'
           << gamma_maximum_difference << '\t'
           << (gamma_mismatches == 0 ? "PASS" : "FAIL") << '\n';
    report << "cosmic\t" << cosmic_mismatches << '\t'
           << cosmic_maximum_difference << '\t'
           << (cosmic_mismatches == 0 ? "PASS" : "FAIL") << '\n';
    if (gamma_mismatches != 0 || cosmic_mismatches != 0) {
      std::cerr << "Event-display histogram validation failed." << std::endl;
      return 1;
    }
    std::cout << "PASS event-display histograms match historical ROOT outputs."
              << std::endl;
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "ERROR: " << error.what() << std::endl;
    return 1;
  }
}
