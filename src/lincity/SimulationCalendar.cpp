#include "SimulationCalendar.hpp"

#include <algorithm>
#include <cassert>

#include "all_buildings.hpp"
#include "lintypes.hpp"
#include "modules/fire.hpp"
#include "modules/shanty.hpp"

namespace {

constexpr int DYNAMIC_FIRE_PERIOD = 0;

constexpr std::array<SimulationEvent, SimulationCalendar::EVENT_COUNT>
  LEGACY_EVENTS = {{
    {SimulationEventId::STATS_MONTHLY, SimulationPhase::STATISTICS,
      NUMOF_DAYS_IN_MONTH, 0, 0},
    {SimulationEventId::STATS_YEARLY, SimulationPhase::STATISTICS,
      NUMOF_DAYS_IN_YEAR, 0, 1},
    {SimulationEventId::DAILY_ECOLOGY, SimulationPhase::PERIODIC, 3, 0, 0},
    {SimulationEventId::SUSTAINABILITY, SimulationPhase::PERIODIC,
      NUMOF_DAYS_IN_YEAR, 0, 1},
    {SimulationEventId::POLLUTION, SimulationPhase::PERIODIC,
      DAYS_PER_POLLUTION, 3, 2},
    {SimulationEventId::RANDOM_FIRE, SimulationPhase::PERIODIC,
      DYNAMIC_FIRE_PERIOD, 9, 3},
    {SimulationEventId::COVERAGE, SimulationPhase::PERIODIC,
      DAYS_BETWEEN_COVER, 75, 4},
    {SimulationEventId::SHANTY, SimulationPhase::PERIODIC,
      DAYS_BETWEEN_SHANTY, 15, 5},
    {SimulationEventId::END_OF_MONTH, SimulationPhase::PERIODIC,
      NUMOF_DAYS_IN_MONTH, NUMOF_DAYS_IN_MONTH - 1, 6},
    {SimulationEventId::END_OF_YEAR, SimulationPhase::PERIODIC,
      NUMOF_DAYS_IN_YEAR, NUMOF_DAYS_IN_YEAR - 1, 7},
  }};

} // namespace

const std::array<SimulationEvent, SimulationCalendar::EVENT_COUNT>&
SimulationCalendar::events() {
  return LEGACY_EVENTS;
}

int SimulationCalendar::period(const SimulationEvent& event, int mapLength) {
  if(event.period != DYNAMIC_FIRE_PERIOD)
    return event.period;

  assert(event.id == SimulationEventId::RANDOM_FIRE);
  assert(mapLength > 0);
  // BUG-26: a fórmula legacy escala ~1/L² e trunca a cada divisão
  // inteira. A partir de ~2683 de lado a segunda divisão zera, e
  // due() trata período 0 como "nunca" — então o incêndio aleatório
  // era desligado de vez em mapas grandes (worldSize aceita até
  // 10000, Config.cpp:229). Clamp em 1 dia: um incêndio não pode
  // acontecer mais de uma vez por dia, e período 0 não significa
  // "todo dia", significa "nenhum".
  return std::max(1,
    DAYS_BETWEEN_FIRES * 100 / mapLength * 100 / mapLength);
}

bool SimulationCalendar::due(
  const SimulationEvent& event, int day, int mapLength) {
  const int eventPeriod = period(event, mapLength);
  if(eventPeriod <= 0)
    return false;
  // BUG-26: a cadência é codificada como `day % period == offset`. Se o
  // período for menor ou igual ao offset, essa congruência não tem
  // solução — `day % 6` nunca vale 9 — e o evento silencia para sempre.
  // Acontece de verdade: a fórmula de incêndio dá 6 dias a L=2000
  // contra offset 9. Período abaixo da granularidade do offset
  // significa "mais frequente que o calendário consegue expressar", e
  // isso degenera em todo dia.
  if(eventPeriod <= event.offset)
    return true;
  return day % eventPeriod == event.offset;
}

const char* SimulationCalendar::name(SimulationEventId id) {
  switch(id) {
    case SimulationEventId::STATS_MONTHLY: return "stats-monthly";
    case SimulationEventId::STATS_YEARLY: return "stats-yearly";
    case SimulationEventId::DAILY_ECOLOGY: return "daily-ecology";
    case SimulationEventId::SUSTAINABILITY: return "sustainability";
    case SimulationEventId::POLLUTION: return "pollution";
    case SimulationEventId::RANDOM_FIRE: return "random-fire";
    case SimulationEventId::COVERAGE: return "coverage";
    case SimulationEventId::SHANTY: return "shanty";
    case SimulationEventId::END_OF_MONTH: return "end-of-month";
    case SimulationEventId::END_OF_YEAR: return "end-of-year";
  }
  return "unknown";
}

const char* SimulationCalendar::phaseName(SimulationPhase phase) {
  switch(phase) {
    case SimulationPhase::STATISTICS: return "statistics";
    case SimulationPhase::PERIODIC: return "periodic";
  }
  return "unknown";
}
