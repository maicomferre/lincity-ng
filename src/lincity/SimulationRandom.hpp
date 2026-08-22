/*
 * REF-07 deterministic simulation random protocol.
 * GPL-2.0-or-later
 */

#ifndef LINCITY_SIMULATION_RANDOM_HPP
#define LINCITY_SIMULATION_RANDOM_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <utility>

#include "MapPoint.hpp"

enum class SimulationProtocol : std::uint8_t {
  LEGACY = 0,
  DETERMINISTIC_V1 = 1,
};

enum class RandomEvent : std::uint8_t {
  SCHEDULE_ORDER = 0,
  ECOLOGY,
  POLLUTION,
  FIRE,
  SHANTY,
  DEMOGRAPHY,
  ANNUAL_RULES,
  ROCKET,
  MAP_GENERATION,
  CONSTRUCTION_STATE,
  WASTE_FIRE,
  COUNT,
};

class RandomContext {
public:
  RandomContext(std::uint64_t seed, RandomEvent event, std::int64_t day,
    MapPoint point = MapPoint(), std::uint64_t ordinal = 0);

  std::uint64_t next();
  std::uint64_t bounded(std::uint64_t upperExclusive);
  bool chance(std::uint64_t numerator, std::uint64_t denominator);
  int normalApprox(int standardDeviation);

  template<typename RandomIt>
  void shuffle(RandomIt first, RandomIt last) {
    using difference_type = typename std::iterator_traits<RandomIt>::difference_type;
    const difference_type count = last - first;
    for(difference_type i = count; i > 1; --i) {
      const difference_type j = static_cast<difference_type>(bounded(
        static_cast<std::uint64_t>(i)));
      if(j != i - 1)
        std::iter_swap(first + (i - 1), first + j);
    }
  }

private:
  std::array<std::uint64_t, 4> state_;
};

class SimulationRandom {
public:
  static constexpr std::uint32_t PROTOCOL_VERSION = 1;

  SimulationRandom(SimulationProtocol protocol = SimulationProtocol::LEGACY,
    std::uint64_t seed = 0);

  SimulationProtocol protocol() const noexcept { return protocol_; }
  std::uint64_t seed() const noexcept { return seed_; }
  RandomContext context(RandomEvent event, std::int64_t day,
    MapPoint point = MapPoint(), std::uint64_t ordinal = 0) const;

private:
  SimulationProtocol protocol_;
  std::uint64_t seed_;
};

class CosmeticRandom {
public:
  explicit CosmeticRandom(std::uint64_t seed = 0);
  std::uint64_t bounded(std::uint64_t upperExclusive);

private:
  RandomContext context_;
};

const char* simulationProtocolName(SimulationProtocol protocol);
const char* randomEventName(RandomEvent event);

#endif
