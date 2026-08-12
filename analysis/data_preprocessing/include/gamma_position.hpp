#ifndef CSHINE_GAMMA_GAMMA_POSITION_HPP
#define CSHINE_GAMMA_GAMMA_POSITION_HPP

// Portable M10 geometry definition migrated from DP-S702.  Coordinates are
// expressed relative to a reconstructed shower centre in crystal-pitch units.

#include <utility>

namespace cshine_gamma {

constexpr unsigned int kGammaCrystalCount = 15U;

std::pair<double, double> RelativeCrystalCoordinates(
    unsigned short centre,
    unsigned short target);

}  // namespace cshine_gamma

#endif  // CSHINE_GAMMA_GAMMA_POSITION_HPP
