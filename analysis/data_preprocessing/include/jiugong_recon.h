#ifndef CSHINE_GAMMA_JIUGONG_RECON_H
#define CSHINE_GAMMA_JIUGONG_RECON_H

// Direct migration of DP-S307.  The global class and function names, result
// layout, energy ordering, geometry, and boundary inequalities are retained
// because historical ROOT trees store this exact result type.

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <type_traits>
#include <utility>
#include <vector>

class jiugong_recon_result_t
    : public std::pair<unsigned short, double>,
      public std::map<unsigned short, double> {
 public:
  using recon_result_t = std::pair<unsigned short, double>;
  using joined_crystals_t = std::map<unsigned short, double>;

  jiugong_recon_result_t()
      : recon_result_t(0, 0.0), joined_crystals_t(), center(-1) {}

  jiugong_recon_result_t(unsigned short center_value, double energy)
      : recon_result_t(1, energy),
        joined_crystals_t({{center_value, energy}}),
        center(static_cast<short>(center_value)) {}

  jiugong_recon_result_t(
      unsigned short center_value,
      double energy,
      std::map<unsigned short, double> joined_crystals)
      : recon_result_t(joined_crystals.size(), energy),
        joined_crystals_t(joined_crystals),
        center(static_cast<short>(center_value)) {}

  virtual ~jiugong_recon_result_t() noexcept = default;

 protected:
  short center;

 public:
  short GetCenter() const noexcept { return center; }
  unsigned short GetMultiplicity() const noexcept {
    return recon_result_t::first;
  }
  double GetEnergy() const noexcept { return recon_result_t::second; }
  const joined_crystals_t& GetJoinedCrystals() const noexcept { return *this; }

  std::pair<joined_crystals_t::iterator, bool> push_crystal(
      unsigned short crystal,
      double new_energy) {
    std::pair<joined_crystals_t::iterator, bool> inserted =
        joined_crystals_t::emplace(crystal, new_energy);
    if (inserted.second) {
      ++recon_result_t::first;
      recon_result_t::second += new_energy;
    } else {
      recon_result_t::second -= inserted.first->second;
      inserted.first->second = new_energy;
      recon_result_t::second += inserted.first->second;
    }
    return inserted;
  }
};

// Returns a map from the candidate CsI index to its reconstructed shower.
// An entry with GetCenter()==-1 is a historical placeholder for a signal that
// lies within 100 ns of a stronger center but is not merged within 50 ns.
template <class EnergyArray, class TimeArray>
std::map<unsigned short, jiugong_recon_result_t> jiugong_recon(
    const EnergyArray& energy,
    const TimeArray& time) {
  constexpr std::size_t kCrystalCount = 15;
  constexpr double kTimeDifferenceNs = 50.0;
  constexpr double kEnergyLowCutMeV = 1.0;
  constexpr double kEnergyThresholdMeV = 1.0;

  std::map<unsigned short, jiugong_recon_result_t> result;

  // Clockwise order beginning with the right-hand neighbor.
  constexpr short neighbor[kCrystalCount][4] = {
      {-1, -1, 1, 4},   {0, -1, 2, 5},    {1, -1, 3, 6},
      {2, -1, -1, 7},   {-1, 0, 5, 8},    {4, 1, 6, 9},
      {5, 2, 7, 10},    {6, 3, -1, 11},   {-1, 4, 9, -1},
      {8, 5, 10, 13},   {9, 6, 11, 14},   {10, 7, -1, 12},
      {14, 11, -1, -1}, {-1, 9, 14, -1},  {13, 10, 12, -1}};

  // Clockwise order beginning with the lower-right diagonal neighbor.
  constexpr short diagonal[kCrystalCount][4] = {
      {-1, -1, 5, -1}, {-1, -1, 6, 4},  {-1, -1, 7, 5},
      {-1, -1, -1, 6}, {-1, 1, 9, -1},  {0, 2, 10, 8},
      {1, 3, 9, 11},   {2, -1, -1, 10}, {-1, 5, 13, -1},
      {4, 6, 14, -1},  {5, 7, 12, 13},  {6, -1, -1, 14},
      {10, -1, -1, -1},{8, 10, -1, -1}, {9, 11, -1, -1}};

  std::vector<unsigned short> centers;
  centers.reserve(kCrystalCount);
  unsigned short sorted_crystals[kCrystalCount];
  for (unsigned short crystal = 0; crystal < kCrystalCount; ++crystal) {
    sorted_crystals[crystal] = crystal;
  }

  if (*std::max_element(std::begin(energy), std::end(energy)) <
      kEnergyThresholdMeV) {
    return result;
  }
  std::sort(
      std::begin(sorted_crystals), std::end(sorted_crystals),
      [&energy](unsigned short left, unsigned short right) {
        return energy[left] > energy[right];
      });

  for (unsigned short rank = 0;
       rank < kCrystalCount && energy[sorted_crystals[rank]] >= kEnergyLowCutMeV;
       ++rank) {
    const unsigned short current = sorted_crystals[rank];
    const double current_time = time[current];
    if (std::isnan(current_time)) {
      continue;
    }
    bool repeat = false;
    bool not_good_center = false;
    for (std::vector<unsigned short>::const_iterator existing = centers.begin();
         existing != centers.end(); ++existing) {
      if (current == *existing) {
        repeat = true;
        break;
      }
      const double existing_time = time[*existing];
      if (std::isnan(existing_time)) {
        continue;
      }
      const double difference = std::abs(existing_time - current_time);
      if (difference <= 2.0 * kTimeDifferenceNs) {
        const bool spatial_neighbor =
            std::find(std::begin(neighbor[*existing]),
                      std::end(neighbor[*existing]), current) !=
                std::end(neighbor[*existing]) ||
            std::find(std::begin(diagonal[*existing]),
                      std::end(diagonal[*existing]), current) !=
                std::end(diagonal[*existing]);
        if (spatial_neighbor && difference <= kTimeDifferenceNs) {
          repeat = true;
        } else {
          not_good_center = true;
        }
        break;
      }
    }
    if (repeat) {
      continue;
    }
    if (not_good_center) {
      result[current] = jiugong_recon_result_t();
      continue;
    }
    centers.push_back(current);
  }

  for (std::vector<unsigned short>::const_iterator center = centers.begin();
       center != centers.end(); ++center) {
    const double center_time = time[*center];
    const double center_energy = energy[*center];
    result[*center] = jiugong_recon_result_t(*center, center_energy);
    jiugong_recon_result_t& reconstructed = result[*center];
    for (const short crystal : neighbor[*center]) {
      if (crystal < 0 || std::isnan(time[crystal]) ||
          std::abs(time[crystal] - center_time) > kTimeDifferenceNs ||
          energy[crystal] < kEnergyLowCutMeV) {
        continue;
      }
      reconstructed.push_crystal(static_cast<unsigned short>(crystal),
                                 energy[crystal]);
    }
    for (const short crystal : diagonal[*center]) {
      if (crystal < 0 || std::isnan(time[crystal]) ||
          std::abs(time[crystal] - center_time) > kTimeDifferenceNs ||
          energy[crystal] < kEnergyLowCutMeV) {
        continue;
      }
      reconstructed.push_crystal(static_cast<unsigned short>(crystal),
                                 energy[crystal]);
    }
  }
  return result;
}

#endif  // CSHINE_GAMMA_JIUGONG_RECON_H
