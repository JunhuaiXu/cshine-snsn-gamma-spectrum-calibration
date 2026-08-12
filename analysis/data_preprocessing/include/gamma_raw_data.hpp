#ifndef CSHINE_GAMMA_RAW_DATA_HPP
#define CSHINE_GAMMA_RAW_DATA_HPP

// Provenance: DP-S401 and DP-S502, the historical timeFigs and production
// gamma_data.hh adapters.  Their branch mappings are identical; they differ
// only in const/static interface qualifiers.

#include <sstream>
#include <string>

namespace cshine_gamma {

inline unsigned int GammaCrystalCount() { return 15U; }
inline unsigned int GammaAdcStorageCount() { return 32U; }
inline unsigned int GammaTdcStorageCount() { return 32U; }
inline unsigned int GammaTriggerTdcStorageCount() { return 32U; }
inline unsigned int T0ChannelCount() { return 4U; }
inline unsigned int VetoChannelCount() { return 3U; }

inline std::string RawGammaBranchName(unsigned int crystal,
                                      const char* suffix) {
  std::ostringstream name;
  name << "GAMMA" << crystal + 1U << suffix;
  return name.str();
}

inline std::string RawLowGainBranchName(unsigned int crystal) {
  return RawGammaBranchName(crystal, "_LOW_E");
}

inline std::string RawHighGainBranchName(unsigned int crystal) {
  return RawGammaBranchName(crystal, "_HIGH_E");
}

inline std::string RawGammaTdcBranchName(unsigned int crystal) {
  return RawGammaBranchName(crystal, "_T");
}

inline const char* RawGammaTriggerTdcBranchName() {
  return "TDC_Gamma_Trig";
}

inline std::string RawT0TdcBranchName(unsigned int channel) {
  std::ostringstream name;
  name << "T0" << channel + 1U << "_T";
  return name.str();
}

inline std::string RawVetoAdcBranchName(unsigned int channel) {
  std::ostringstream name;
  name << "Veto" << channel + 1U << "_E";
  return name.str();
}

inline std::string RawVetoTdcBranchName(unsigned int channel) {
  std::ostringstream name;
  name << "Veto" << channel + 1U << "_T";
  return name.str();
}

}  // namespace cshine_gamma

#endif  // CSHINE_GAMMA_RAW_DATA_HPP
