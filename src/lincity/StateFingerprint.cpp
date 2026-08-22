#include "StateFingerprint.hpp"

#include <algorithm>
#include <array>
#include <iomanip>
#include <sstream>
#include <utility>

#include "world.hpp"
#include "modules/coalmine.hpp"
#include "modules/commune.hpp"
#include "modules/market.hpp"
#include "modules/school.hpp"
#include "modules/windmill.hpp"
#include "modules/windpower.hpp"
#include "xmlloadsave.hpp"

namespace {

class StableHash128 {
public:
  void update(std::string_view bytes) {
    for(const unsigned char byte : bytes) {
      low ^= byte;
      low *= 1099511628211ULL;
      high ^= static_cast<std::uint64_t>(byte) + 0x9eU;
      high *= 14029467366897019727ULL;
    }
    length += bytes.size();
  }

  void updateU64(std::uint64_t value) {
    std::array<char, 8> bytes{};
    for(std::size_t index = 0; index < bytes.size(); ++index) {
      bytes[index] = static_cast<char>(value & 0xffU);
      value >>= 8U;
    }
    update(std::string_view(bytes.data(), bytes.size()));
  }

  StateFingerprint128 finish() const {
    return StateFingerprint128{
      avalanche(high ^ rotateLeft(length, 29U)),
      avalanche(low ^ rotateLeft(length, 17U))
    };
  }

private:
  static std::uint64_t rotateLeft(std::uint64_t value, unsigned int count) {
    return (value << count) | (value >> (64U - count));
  }

  static std::uint64_t avalanche(std::uint64_t value) {
    value ^= value >> 33U;
    value *= 0xff51afd7ed558ccdULL;
    value ^= value >> 33U;
    value *= 0xc4ceb9fe1a85ec53ULL;
    value ^= value >> 33U;
    return value;
  }

  std::uint64_t high = 7809847782465536322ULL;
  std::uint64_t low = 14695981039346656037ULL;
  std::uint64_t length = 0;
};

void updateFramed(StableHash128& hash, std::string_view bytes) {
  hash.updateU64(bytes.size());
  hash.update(bytes);
}

std::string serializePollutedQueue(const Map& map) {
  std::string result;
  result.reserve(map.polluted.size() * 24U + 24U);
  result += std::to_string(map.polluted.size());
  result.push_back('\n');
  for(const MapPoint point : map.polluted) {
    result += std::to_string(point.x);
    result.push_back(',');
    result += std::to_string(point.y);
    result.push_back('\n');
  }
  return result;
}

void appendField(std::string& result, std::string_view name, int value) {
  result.append(name);
  result.push_back('=');
  result.append(std::to_string(value));
  result.push_back('\n');
}

std::string serializeLegacyTrajectoryExtras(const Map& map) {
  std::string result;
  result.reserve(map.constructions.size() * 32U);
  for(const Construction* construction : map.constructions) {
    if(construction->isDead())
      continue;

    std::string fields;
    if(const auto* mine = dynamic_cast<const Coalmine*>(construction)) {
      appendField(fields, "mine_cur_x", mine->mine_cur.x);
      appendField(fields, "mine_cur_y", mine->mine_cur.y);
      appendField(fields, "current_coal_reserve", mine->current_coal_reserve);
    }
    else if(const auto* commune = dynamic_cast<const Commune*>(construction)) {
      appendField(fields, "monthly_stuff_made", commune->monthly_stuff_made);
      appendField(fields, "lazy_months", commune->lazy_months);
    }
    else if(const auto* market = dynamic_cast<const Market*>(construction)) {
      appendField(fields, "labor", market->labor);
    }
    else if(const auto* school = dynamic_cast<const School*>(construction)) {
      appendField(fields, "working_days", school->working_days);
      appendField(fields, "busy", school->busy);
    }
    else if(const auto* windmill = dynamic_cast<const Windmill*>(construction)) {
      appendField(fields, "paid", windmill->paid ? 1 : 0);
    }
    else if(const auto* windpower = dynamic_cast<const Windpower*>(construction)) {
      appendField(fields, "paid", windpower->paid ? 1 : 0);
    }

    if(fields.empty())
      continue;
    appendField(result, "group", construction->constructionGroup->group);
    appendField(result, "map_x", construction->point.x);
    appendField(result, "map_y", construction->point.y);
    result.append(fields);
  }
  return result;
}

} // namespace

std::string StateFingerprint128::hex() const {
  std::ostringstream stream;
  stream << std::hex << std::setfill('0')
    << std::setw(16) << high
    << std::setw(16) << low;
  return stream.str();
}

StateSnapshot::StateSnapshot(std::vector<StateSection> sections)
  : stateSections(std::move(sections)) {}

StateFingerprint128 StateSnapshot::fingerprint() const {
  StableHash128 hash;
  updateFramed(hash, "lincity-ng-state-v1");
  hash.updateU64(stateSections.size());
  for(const StateSection& section : stateSections) {
    updateFramed(hash, section.name);
    updateFramed(hash, section.bytes);
  }
  return hash.finish();
}

std::optional<StateDifference> StateSnapshot::firstDifference(
    const StateSnapshot& other) const {
  const std::size_t commonSections = std::min(
    stateSections.size(), other.stateSections.size());
  for(std::size_t sectionIndex = 0; sectionIndex < commonSections;
      ++sectionIndex) {
    const StateSection& leftSection = stateSections[sectionIndex];
    const StateSection& rightSection = other.stateSections[sectionIndex];
    if(leftSection.name != rightSection.name) {
      return StateDifference{
        "section-name", sectionIndex,
        std::nullopt, std::nullopt
      };
    }

    const std::size_t commonBytes = std::min(
      leftSection.bytes.size(), rightSection.bytes.size());
    for(std::size_t offset = 0; offset < commonBytes; ++offset) {
      const auto left = static_cast<unsigned char>(leftSection.bytes[offset]);
      const auto right = static_cast<unsigned char>(rightSection.bytes[offset]);
      if(left != right)
        return StateDifference{leftSection.name, offset, left, right};
    }
    if(leftSection.bytes.size() != rightSection.bytes.size()) {
      return StateDifference{
        leftSection.name,
        commonBytes,
        commonBytes < leftSection.bytes.size()
          ? std::optional<unsigned char>(static_cast<unsigned char>(
              leftSection.bytes[commonBytes]))
          : std::nullopt,
        commonBytes < rightSection.bytes.size()
          ? std::optional<unsigned char>(static_cast<unsigned char>(
              rightSection.bytes[commonBytes]))
          : std::nullopt
      };
    }
  }

  if(stateSections.size() != other.stateSections.size()) {
    return StateDifference{
      "section-count", commonSections, std::nullopt, std::nullopt
    };
  }
  return std::nullopt;
}

StateFingerprint128 fingerprintBytes(std::string_view bytes) {
  StableHash128 hash;
  updateFramed(hash, "lincity-ng-bytes-v1");
  updateFramed(hash, bytes);
  return hash.finish();
}

StateSnapshot captureStateSnapshot(const World& world) {
  std::vector<StateSection> sections;
  sections.reserve(3);
  sections.push_back({
    "gameplay-xml-v1", serializeWorldGameplayState(world)
  });
  sections.push_back({
    "legacy-polluted-queue-v1", serializePollutedQueue(world.map)
  });
  sections.push_back({
    "legacy-trajectory-extras-v1",
    serializeLegacyTrajectoryExtras(world.map)
  });
  return StateSnapshot(std::move(sections));
}
