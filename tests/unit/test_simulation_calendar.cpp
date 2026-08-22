#include "../tiny_test.hpp"

#include <vector>

#include "lincity/SimulationCalendar.hpp"

namespace {

std::vector<SimulationEventId> dueEvents(SimulationPhase phase, int day) {
  std::vector<SimulationEventId> result;
  for(const SimulationEvent& event : SimulationCalendar::events())
    if(event.phase == phase && SimulationCalendar::due(event, day, 100))
      result.push_back(event.id);
  return result;
}

} // namespace

TTEST(legacy_calendar_boundaries_are_explicit) {
  TCHECK_EQ(dueEvents(SimulationPhase::STATISTICS, 0).size(), std::size_t(2));
  TCHECK_EQ(dueEvents(SimulationPhase::PERIODIC, 0).size(), std::size_t(2));

  const auto day3 = dueEvents(SimulationPhase::PERIODIC, 3);
  TCHECK_EQ(day3.size(), std::size_t(2));
  TCHECK(day3[0] == SimulationEventId::DAILY_ECOLOGY);
  TCHECK(day3[1] == SimulationEventId::POLLUTION);

  TCHECK(dueEvents(SimulationPhase::PERIODIC, 14).empty());

  const auto day75 = dueEvents(SimulationPhase::PERIODIC, 75);
  TCHECK_EQ(day75.size(), std::size_t(2));
  TCHECK(day75[0] == SimulationEventId::DAILY_ECOLOGY);
  TCHECK(day75[1] == SimulationEventId::COVERAGE);

  const auto day99 = dueEvents(SimulationPhase::PERIODIC, 99);
  TCHECK_EQ(day99.size(), std::size_t(2));
  TCHECK(day99[0] == SimulationEventId::DAILY_ECOLOGY);
  TCHECK(day99[1] == SimulationEventId::END_OF_MONTH);

  TCHECK_EQ(dueEvents(SimulationPhase::STATISTICS, 100).size(), std::size_t(1));
  TCHECK_EQ(dueEvents(SimulationPhase::STATISTICS, 300).size(), std::size_t(1));
  TCHECK_EQ(dueEvents(SimulationPhase::PERIODIC, 300).size(), std::size_t(1));

  const auto day1199 = dueEvents(SimulationPhase::PERIODIC, 1199);
  TCHECK_EQ(day1199.size(), std::size_t(2));
  TCHECK(day1199[0] == SimulationEventId::END_OF_MONTH);
  TCHECK(day1199[1] == SimulationEventId::END_OF_YEAR);

  TCHECK_EQ(dueEvents(SimulationPhase::STATISTICS, 1200).size(), std::size_t(2));
  const auto day1200 = dueEvents(SimulationPhase::PERIODIC, 1200);
  TCHECK_EQ(day1200.size(), std::size_t(2));
  TCHECK(day1200[0] == SimulationEventId::DAILY_ECOLOGY);
  TCHECK(day1200[1] == SimulationEventId::SUSTAINABILITY);
}

TTEST(legacy_calendar_keeps_dynamic_fire_and_named_priority) {
  const auto day9 = dueEvents(SimulationPhase::PERIODIC, 9);
  TCHECK_EQ(day9.size(), std::size_t(2));
  TCHECK(day9[0] == SimulationEventId::DAILY_ECOLOGY);
  TCHECK(day9[1] == SimulationEventId::RANDOM_FIRE);

  int statisticsPriority = -1;
  int periodicPriority = -1;
  for(const SimulationEvent& event : SimulationCalendar::events()) {
    int& previous = event.phase == SimulationPhase::STATISTICS
      ? statisticsPriority : periodicPriority;
    TCHECK(event.priority > previous);
    previous = event.priority;
    TCHECK(SimulationCalendar::name(event.id) != nullptr);
    if(event.id == SimulationEventId::RANDOM_FIRE) {
      TCHECK(!SimulationCalendar::due(event, 609, 100));
      TCHECK(SimulationCalendar::due(event, 609, 200));
    }
  }
}
