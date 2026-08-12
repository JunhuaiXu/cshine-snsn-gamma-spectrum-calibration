#ifndef CSHINE_GAMMA_SPATIAL_SPREAD_HPP
#define CSHINE_GAMMA_SPATIAL_SPREAD_HPP

// Portable M10 observable definitions migrated from DP-S703 and DP-S704.

#include "jiugong_recon.h"

#include <utility>

namespace cshine_gamma {

struct SpatialSpread {
  double delta_x_pitch;
  double delta_y_pitch;
  double sigma_x_pitch;
  double sigma_y_pitch;
};

SpatialSpread CalculateSpatialSpread(
    const jiugong_recon_result_t& reconstruction);

}  // namespace cshine_gamma

#endif  // CSHINE_GAMMA_SPATIAL_SPREAD_HPP
