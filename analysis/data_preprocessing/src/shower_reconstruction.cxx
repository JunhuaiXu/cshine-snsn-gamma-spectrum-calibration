// Direct selection-layer migration of DP-S600 around DP-S307/DP-S308.

#include "shower_reconstruction.h"

#include <algorithm>
#include <stdexcept>

namespace cshine_gamma {
namespace {

template <std::size_t Size>
bool Contains(const std::array<unsigned short, Size>& values,
              unsigned short value) {
  return std::find(values.begin(), values.end(), value) != values.end();
}

}  // namespace

ShowerReconstructionDefinition HistoricalShowerReconstructionDefinition() {
  ShowerReconstructionDefinition definition;
  definition.crystal_count = 15U;
  definition.crystal_energy_threshold_mev = 1.0;
  definition.neighbor_time_window_ns = 50.0;
  definition.separate_core_time_ns = 100.0;
  definition.central_cores = {{5U, 6U, 9U, 10U}};
  definition.side_cores = {{4U, 7U, 8U, 11U, 13U, 14U}};
  definition.lower_edge_cores = {{1U, 2U}};
  definition.corner_crystals = {{0U, 3U, 12U}};
  definition.veto_count = 3U;
  definition.veto_tdc_min_exclusive = 100U;
  definition.veto_tdc_max_exclusive = 4000U;
  return definition;
}

void ValidateShowerReconstructionDefinition(
    const ShowerReconstructionDefinition& definition) {
  const ShowerReconstructionDefinition historical =
      HistoricalShowerReconstructionDefinition();
  if (definition.crystal_count != historical.crystal_count ||
      definition.crystal_energy_threshold_mev !=
          historical.crystal_energy_threshold_mev ||
      definition.neighbor_time_window_ns !=
          historical.neighbor_time_window_ns ||
      definition.separate_core_time_ns !=
          historical.separate_core_time_ns ||
      definition.central_cores != historical.central_cores ||
      definition.side_cores != historical.side_cores ||
      definition.lower_edge_cores != historical.lower_edge_cores ||
      definition.corner_crystals != historical.corner_crystals ||
      definition.veto_count != historical.veto_count ||
      definition.veto_tdc_min_exclusive !=
          historical.veto_tdc_min_exclusive ||
      definition.veto_tdc_max_exclusive !=
          historical.veto_tdc_max_exclusive) {
    throw std::invalid_argument("historical shower-reconstruction definition changed");
  }
}

CrystalRole ClassifyCrystal(unsigned short crystal) {
  const ShowerReconstructionDefinition definition =
      HistoricalShowerReconstructionDefinition();
  if (crystal >= definition.crystal_count) {
    throw std::out_of_range("CsI crystal index is outside 0--14");
  }
  if (Contains(definition.central_cores, crystal)) {
    return CrystalRole::kCentral;
  }
  if (Contains(definition.side_cores, crystal)) {
    return CrystalRole::kMainSide;
  }
  if (Contains(definition.lower_edge_cores, crystal)) {
    return CrystalRole::kLowerEdgeWithoutVeto;
  }
  return CrystalRole::kCorner;
}

bool IsValidReconstruction(const jiugong_recon_result_t& result) {
  return result.GetCenter() >= 0 && result.GetMultiplicity() > 0U;
}

bool IsMainSpectrumCandidate(const jiugong_recon_result_t& result,
                             unsigned short veto_signal_count) {
  if (!IsValidReconstruction(result)) {
    return false;
  }
  const CrystalRole role =
      ClassifyCrystal(static_cast<unsigned short>(result.GetCenter()));
  if (role == CrystalRole::kCentral) {
    return true;
  }
  return role == CrystalRole::kMainSide && veto_signal_count == 0U;
}

unsigned short CountVetoSignals(
    const std::array<int, 3>& veto_tdc) {
  const ShowerReconstructionDefinition definition =
      HistoricalShowerReconstructionDefinition();
  unsigned short count = 0U;
  for (std::array<int, 3>::const_iterator value = veto_tdc.begin();
       value != veto_tdc.end(); ++value) {
    if (*value > definition.veto_tdc_min_exclusive &&
        *value < definition.veto_tdc_max_exclusive) {
      ++count;
    }
  }
  return count;
}

}  // namespace cshine_gamma
