/*
 * REF-07 deterministic simulation random protocol.
 * GPL-2.0-or-later
 */

#include "SimulationRandom.hpp"

#include <cassert>
#include <climits>
#include <cstdint>

namespace {

constexpr std::uint64_t kProtocolTag = 0x4c4e47524e470001ULL;
constexpr std::uint64_t kCosmeticTag = 0x434f534d45544943ULL;

std::uint64_t splitmix64(std::uint64_t& value) {
  value += 0x9e3779b97f4a7c15ULL;
  std::uint64_t result = value;
  result = (result ^ (result >> 30)) * 0xbf58476d1ce4e5b9ULL;
  result = (result ^ (result >> 27)) * 0x94d049bb133111ebULL;
  return result ^ (result >> 31);
}

constexpr std::uint64_t rotateLeft(std::uint64_t value, int shift) {
  return (value << shift) | (value >> (64 - shift));
}

void mix(std::uint64_t& key, std::uint64_t value) {
  key ^= value + 0x9e3779b97f4a7c15ULL + (key << 6) + (key >> 2);
  key = splitmix64(key);
}

} // namespace

RandomContext::RandomContext(std::uint64_t seed, RandomEvent event,
  std::int64_t day, MapPoint point, std::uint64_t ordinal)
{
  std::uint64_t key = kProtocolTag;
  mix(key, SimulationRandom::PROTOCOL_VERSION);
  mix(key, seed);
  mix(key, static_cast<std::uint64_t>(event));
  mix(key, static_cast<std::uint64_t>(day));
  mix(key, static_cast<std::uint32_t>(point.x));
  mix(key, static_cast<std::uint32_t>(point.y));
  mix(key, ordinal);
  for(std::uint64_t& word : state_)
    word = splitmix64(key);
  if((state_[0] | state_[1] | state_[2] | state_[3]) == 0)
    state_[0] = 1;
}

std::uint64_t RandomContext::next() {
  const std::uint64_t result = rotateLeft(state_[1] * 5, 7) * 9;
  const std::uint64_t temporary = state_[1] << 17;
  state_[2] ^= state_[0];
  state_[3] ^= state_[1];
  state_[1] ^= state_[2];
  state_[0] ^= state_[3];
  state_[2] ^= temporary;
  state_[3] = rotateLeft(state_[3], 45);
  return result;
}

std::uint64_t RandomContext::bounded(std::uint64_t upperExclusive) {
  assert(upperExclusive > 0);
  const std::uint64_t threshold = -upperExclusive % upperExclusive;
  for(;;) {
    const std::uint64_t value = next();
    if(value >= threshold)
      return value % upperExclusive;
  }
}

bool RandomContext::chance(std::uint64_t numerator, std::uint64_t denominator) {
  assert(denominator > 0);
  if(numerator == 0)
    return false;
  if(numerator >= denominator)
    return true;
  return bounded(denominator) < numerator;
}

int RandomContext::normalApprox(int standardDeviation) {
  assert(standardDeviation >= 0);
  std::int64_t sum = 0;
  for(int i = 0; i < 12; ++i)
    sum += static_cast<std::int64_t>(bounded(65536));
  const std::int64_t centered = sum - 12LL * 65535 / 2;
  return static_cast<int>(centered * standardDeviation / 65536);
}

SimulationRandom::SimulationRandom(SimulationProtocol protocol,
  std::uint64_t seed) :
  protocol_(protocol), seed_(seed)
{}

RandomContext SimulationRandom::context(RandomEvent event, std::int64_t day,
  MapPoint point, std::uint64_t ordinal) const
{
  assert(protocol_ == SimulationProtocol::DETERMINISTIC_V1);
  return RandomContext(seed_, event, day, point, ordinal);
}

CosmeticRandom::CosmeticRandom(std::uint64_t seed) :
  context_(seed ^ kCosmeticTag, RandomEvent::CONSTRUCTION_STATE, 0)
{}

std::uint64_t CosmeticRandom::bounded(std::uint64_t upperExclusive) {
  return context_.bounded(upperExclusive);
}

const char* simulationProtocolName(SimulationProtocol protocol) {
  switch(protocol) {
    case SimulationProtocol::LEGACY: return "Legacy";
    case SimulationProtocol::DETERMINISTIC_V1: return "DeterministicV1";
  }
  assert(false);
  return "unknown";
}

const char* randomEventName(RandomEvent event) {
  switch(event) {
    case RandomEvent::SCHEDULE_ORDER: return "schedule-order";
    case RandomEvent::ECOLOGY: return "ecology";
    case RandomEvent::POLLUTION: return "pollution";
    case RandomEvent::FIRE: return "fire";
    case RandomEvent::SHANTY: return "shanty";
    case RandomEvent::DEMOGRAPHY: return "demography";
    case RandomEvent::ANNUAL_RULES: return "annual-rules";
    case RandomEvent::ROCKET: return "rocket";
    case RandomEvent::MAP_GENERATION: return "map-generation";
    case RandomEvent::CONSTRUCTION_STATE: return "construction-state";
    case RandomEvent::WASTE_FIRE: return "waste-fire";
    case RandomEvent::COUNT: break;
  }
  assert(false);
  return "unknown";
}
