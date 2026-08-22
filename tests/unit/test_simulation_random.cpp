#include "../tiny_test.hpp"

#include <array>
#include <cstdint>
#include <vector>

#include "lincity/SimulationRandom.hpp"

TTEST(simulation_random_v1_has_frozen_golden_vectors) {
  SimulationRandom random(SimulationProtocol::DETERMINISTIC_V1,
    0x0123456789abcdefULL);
  RandomContext context = random.context(RandomEvent::FIRE, 1199,
    MapPoint(17, 42), 7);

  const std::array<std::uint64_t, 5> expected = {
    10628717021136149627ULL,
    7267409691835076285ULL,
    14576412208343309310ULL,
    14467237128798231535ULL,
    7419483044747453848ULL
  };
  for(const std::uint64_t value : expected)
    TCHECK_EQ(context.next(), value);
}

TTEST(simulation_random_v1_bounds_chance_shuffle_and_domains) {
  SimulationRandom random(SimulationProtocol::DETERMINISTIC_V1, 1234567);
  RandomContext bounded = random.context(RandomEvent::ECOLOGY, 300);
  for(int i = 0; i < 1000; ++i)
    TCHECK(bounded.bounded(17) < 17);

  RandomContext never = random.context(RandomEvent::FIRE, 300);
  RandomContext always = random.context(RandomEvent::FIRE, 300);
  TCHECK(!never.chance(0, 100));
  TCHECK(always.chance(100, 100));

  std::vector<int> left{0, 1, 2, 3, 4, 5, 6, 7};
  std::vector<int> right = left;
  random.context(RandomEvent::SCHEDULE_ORDER, 75).shuffle(
    left.begin(), left.end());
  random.context(RandomEvent::SCHEDULE_ORDER, 75).shuffle(
    right.begin(), right.end());
  TCHECK(left == right);
  TCHECK(left != std::vector<int>({0, 1, 2, 3, 4, 5, 6, 7}));

  RandomContext ecology = random.context(RandomEvent::ECOLOGY, 99);
  RandomContext fire = random.context(RandomEvent::FIRE, 99);
  TCHECK(ecology.next() != fire.next());
}
