#include "gamma_position.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace cshine_gamma {
namespace {

constexpr short kNeighbor[kGammaCrystalCount][4] = {
    {-1, -1, 1, 4},   {0, -1, 2, 5},    {1, -1, 3, 6},
    {2, -1, -1, 7},   {-1, 0, 5, 8},    {4, 1, 6, 9},
    {5, 2, 7, 10},    {6, 3, -1, 11},   {-1, 4, 9, -1},
    {8, 5, 10, 13},   {9, 6, 11, 14},   {10, 7, -1, 12},
    {14, 11, -1, -1}, {-1, 9, 14, -1},  {13, 10, 12, -1}};

constexpr short kDiagonal[kGammaCrystalCount][4] = {
    {-1, -1, 5, -1}, {-1, -1, 6, 4},  {-1, -1, 7, 5},
    {-1, -1, -1, 6}, {-1, 1, 9, -1},  {0, 2, 10, 8},
    {1, 3, 9, 11},   {2, -1, -1, 10}, {-1, 5, 13, -1},
    {4, 6, 14, -1},  {5, 7, 12, 13},  {6, -1, -1, 14},
    {10, -1, -1, -1},{8, 10, -1, -1}, {9, 11, -1, -1}};

constexpr double kNeighborX[4] = {1.0, 0.0, -1.0, 0.0};
constexpr double kNeighborY[4] = {0.0, -1.0, 0.0, 1.0};
constexpr double kDiagonalX[4] = {1.0, -1.0, -1.0, 1.0};
constexpr double kDiagonalY[4] = {-1.0, -1.0, 1.0, 1.0};

}  // namespace

std::pair<double, double> RelativeCrystalCoordinates(
    unsigned short centre,
    unsigned short target) {
  if (centre >= kGammaCrystalCount || target >= kGammaCrystalCount) {
    throw std::out_of_range("crystal index outside the 15-unit geometry");
  }
  if (centre == target) {
    return std::make_pair(0.0, 0.0);
  }
  for (unsigned int index = 0; index < 4U; ++index) {
    if (kNeighbor[centre][index] == static_cast<short>(target)) {
      return std::make_pair(kNeighborX[index], kNeighborY[index]);
    }
    if (kDiagonal[centre][index] == static_cast<short>(target)) {
      return std::make_pair(kDiagonalX[index], kDiagonalY[index]);
    }
  }
  const double nan = std::numeric_limits<double>::quiet_NaN();
  return std::make_pair(nan, nan);
}

}  // namespace cshine_gamma
