/* Versioned external-command replay for deterministic simulation checks. */

#ifndef LINCITYNG_SIMULATION_REPLAY_HPP
#define LINCITYNG_SIMULATION_REPLAY_HPP

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "MapPoint.hpp"
#include "StateFingerprint.hpp"
#include "world.hpp"

enum class SimulationCommandType {
  BUILD,
  BULLDOZE,
  EVACUATE,
  FLOOD,
  SET_MONEY_RATE,
  COAL_SURVEY,
  LAUNCH_ROCKET,
};

enum class SimulationCommandPhase {
  BEFORE_STEP,
};

enum class SimulationCommandOutcome {
  APPLIED,
  NO_CHANGE,
};

struct SimulationCommand {
  std::uint32_t day = 0;
  std::uint32_t sequence = 0;
  SimulationCommandPhase phase = SimulationCommandPhase::BEFORE_STEP;
  SimulationCommandType type = SimulationCommandType::BUILD;
  MapPoint point;
  int argument0 = 0;
  int argument1 = 0;
  SimulationCommandOutcome expected = SimulationCommandOutcome::APPLIED;
};

struct SimulationCheckpoint {
  std::uint32_t day = 0;
  StateFingerprint128 fingerprint;
};

struct SimulationReplay {
  static constexpr std::uint32_t FORMAT_VERSION = 1;

  std::uint32_t version = FORMAT_VERSION;
  std::string protocol = "legacy";
  std::uint64_t seed = 0;
  std::string origin;
  StateFingerprint128 initialFingerprint;
  std::vector<SimulationCommand> commands;
  std::vector<SimulationCheckpoint> checkpoints;
};

class SimulationReplayError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

SimulationCommandOutcome executeSimulationCommand(
  World& world, const SimulationCommand& command);

void recordSimulationDay(World& world, SimulationReplay& replay,
  std::uint32_t day, std::vector<SimulationCommand> commands);

void replaySimulation(World& world, const SimulationReplay& replay);

std::string serializeSimulationReplay(const SimulationReplay& replay);
SimulationReplay parseSimulationReplay(std::string_view bytes);

#endif
