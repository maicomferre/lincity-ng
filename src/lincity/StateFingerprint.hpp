/*
 * Stable diagnostic fingerprints for REF-07 trajectory characterization.
 */

#ifndef LINCITYNG_STATE_FINGERPRINT_HPP
#define LINCITYNG_STATE_FINGERPRINT_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class World;

struct StateFingerprint128 {
  std::uint64_t high = 0;
  std::uint64_t low = 0;

  bool operator==(const StateFingerprint128& other) const {
    return high == other.high && low == other.low;
  }
  bool operator!=(const StateFingerprint128& other) const {
    return !(*this == other);
  }
  std::string hex() const;
};

struct StateSection {
  std::string name;
  std::string bytes;
};

struct StateDifference {
  std::string section;
  std::size_t offset = 0;
  std::optional<unsigned char> left;
  std::optional<unsigned char> right;
};

class StateSnapshot {
public:
  explicit StateSnapshot(std::vector<StateSection> sections);

  const std::vector<StateSection>& sections() const { return stateSections; }
  StateFingerprint128 fingerprint() const;
  std::optional<StateDifference> firstDifference(
    const StateSnapshot& other) const;

private:
  std::vector<StateSection> stateSections;
};

StateFingerprint128 fingerprintBytes(std::string_view bytes);
StateSnapshot captureStateSnapshot(const World& world);

#endif
