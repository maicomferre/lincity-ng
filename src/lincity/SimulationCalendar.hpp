/* Explicit Legacy simulation calendar. */

#ifndef LINCITYNG_SIMULATION_CALENDAR_HPP
#define LINCITYNG_SIMULATION_CALENDAR_HPP

#include <array>
#include <cstddef>

enum class SimulationPhase {
  STATISTICS,
  PERIODIC,
};

enum class SimulationEventId {
  STATS_MONTHLY,
  STATS_YEARLY,
  DAILY_ECOLOGY,
  SUSTAINABILITY,
  POLLUTION,
  RANDOM_FIRE,
  COVERAGE,
  SHANTY,
  END_OF_MONTH,
  END_OF_YEAR,
};

struct SimulationEvent {
  SimulationEventId id;
  SimulationPhase phase;
  int period;
  int offset;
  int priority;
};

class SimulationCalendar {
public:
  static constexpr std::size_t EVENT_COUNT = 10;

  static const std::array<SimulationEvent, EVENT_COUNT>& events();
  static bool due(const SimulationEvent& event, int day, int mapLength);
  static const char* name(SimulationEventId id);
  static const char* phaseName(SimulationPhase phase);

private:
  static int period(const SimulationEvent& event, int mapLength);
};

#endif
