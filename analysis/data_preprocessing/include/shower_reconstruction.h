#ifndef CSHINE_GAMMA_SHOWER_RECONSTRUCTION_H
#define CSHINE_GAMMA_SHOWER_RECONSTRUCTION_H

// Physics-selection helpers surrounding the historical jiugong_recon result.
// Provenance: DP-S307, DP-S308, DP-S600, and the reviewed Sec. 3.3.3 contract.

#include "jiugong_recon.h"

#include <array>

namespace cshine_gamma {

enum class CrystalRole {
  kCorner,
  kLowerEdgeWithoutVeto,
  kMainSide,
  kCentral
};

struct ShowerReconstructionDefinition {
  unsigned int crystal_count;
  double crystal_energy_threshold_mev;
  double neighbor_time_window_ns;
  double separate_core_time_ns;
  std::array<unsigned short, 4> central_cores;
  std::array<unsigned short, 6> side_cores;
  std::array<unsigned short, 2> lower_edge_cores;
  std::array<unsigned short, 3> corner_crystals;
  unsigned int veto_count;
  unsigned short veto_tdc_min_exclusive;
  unsigned short veto_tdc_max_exclusive;
};

ShowerReconstructionDefinition HistoricalShowerReconstructionDefinition();

void ValidateShowerReconstructionDefinition(
    const ShowerReconstructionDefinition& definition);

CrystalRole ClassifyCrystal(unsigned short crystal);

bool IsValidReconstruction(const jiugong_recon_result_t& result);

bool IsMainSpectrumCandidate(const jiugong_recon_result_t& result,
                             unsigned short veto_signal_count);

unsigned short CountVetoSignals(
    const std::array<int, 3>& veto_tdc);

}  // namespace cshine_gamma

#endif  // CSHINE_GAMMA_SHOWER_RECONSTRUCTION_H
