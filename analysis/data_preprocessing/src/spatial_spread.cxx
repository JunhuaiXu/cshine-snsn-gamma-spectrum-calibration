#include "spatial_spread.hpp"

#include "gamma_position.hpp"

#include <cmath>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace cshine_gamma {

SpatialSpread CalculateSpatialSpread(
    const jiugong_recon_result_t& reconstruction) {
  const double total_energy = reconstruction.GetEnergy();
  const short centre = reconstruction.GetCenter();
  if (centre < 0 || centre >= static_cast<short>(kGammaCrystalCount) ||
      total_energy <= 0.0 || reconstruction.GetJoinedCrystals().empty()) {
    throw std::invalid_argument("spatial spread requires a valid reconstruction");
  }

  typedef std::tuple<double, double, double> WeightedPoint;
  std::vector<WeightedPoint> points;
  double weighted_x = 0.0;
  double weighted_y = 0.0;
  for (jiugong_recon_result_t::joined_crystals_t::const_iterator item =
           reconstruction.GetJoinedCrystals().begin();
       item != reconstruction.GetJoinedCrystals().end(); ++item) {
    const std::pair<double, double> coordinate =
        RelativeCrystalCoordinates(static_cast<unsigned short>(centre),
                                   item->first);
    if (!std::isfinite(coordinate.first) || !std::isfinite(coordinate.second)) {
      throw std::runtime_error(
          "reconstructed crystal is outside the centre's 3x3 neighborhood");
    }
    points.push_back(
        std::make_tuple(coordinate.first, coordinate.second, item->second));
    weighted_x += coordinate.first * item->second;
    weighted_y += coordinate.second * item->second;
  }
  const double mean_x = weighted_x / total_energy;
  const double mean_y = weighted_y / total_energy;
  double absolute_x = 0.0;
  double absolute_y = 0.0;
  double squared_x = 0.0;
  double squared_y = 0.0;
  for (std::vector<WeightedPoint>::const_iterator point = points.begin();
       point != points.end(); ++point) {
    const double x = std::get<0>(*point);
    const double y = std::get<1>(*point);
    const double energy = std::get<2>(*point);
    absolute_x += std::abs(x - mean_x) * energy;
    absolute_y += std::abs(y - mean_y) * energy;
    squared_x += (x - mean_x) * (x - mean_x) * energy * energy;
    squared_y += (y - mean_y) * (y - mean_y) * energy * energy;
  }

  SpatialSpread result;
  result.delta_x_pitch = absolute_x / total_energy;
  result.delta_y_pitch = absolute_y / total_energy;
  result.sigma_x_pitch = std::sqrt(squared_x) / total_energy;
  result.sigma_y_pitch = std::sqrt(squared_y) / total_energy;
  return result;
}

}  // namespace cshine_gamma
