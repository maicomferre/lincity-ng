#include "../tiny_test.hpp"

#include <string>
#include <vector>

#include "lincity/SimulationCalendar.hpp"
#include "lincity/modules/fire.hpp"       // for DAYS_BETWEEN_FIRES

namespace {

std::vector<SimulationEventId> dueEvents(SimulationPhase phase, int day) {
  std::vector<SimulationEventId> result;
  for(const SimulationEvent& event : SimulationCalendar::events())
    if(event.phase == phase && SimulationCalendar::due(event, day, 100))
      result.push_back(event.id);
  return result;
}

const SimulationEvent* findFire() {
  for(const SimulationEvent& event : SimulationCalendar::events())
    if(event.id == SimulationEventId::RANDOM_FIRE)
      return &event;
  return nullptr;
}

/* First / second day the event fires for a given map side, or -1. The
 * calendar offers only due() publicly, so the cadence has to be
 * observed through it. The horizon is the caller's: the fire period
 * grows as 1/L^2 and reaches 9600 days at L=50, so a fixed horizon
 * short enough to stay cheap would miss the second firing entirely. */
int firstDueDay(const SimulationEvent& event, int mapLength, int horizon) {
  for(int day = 0; day < horizon; ++day)
    if(SimulationCalendar::due(event, day, mapLength))
      return day;
  return -1;
}

int secondDueDay(const SimulationEvent& event, int mapLength, int horizon) {
  bool seen = false;
  for(int day = 0; day < horizon; ++day) {
    if(!SimulationCalendar::due(event, day, mapLength))
      continue;
    if(seen)
      return day;
    seen = true;
  }
  return -1;
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

/* BUG-26: a cadencia do incendio aleatorio e codificada como
 * `day % period == offset`, com period = DAYS_BETWEEN_FIRES*10^4/L^2
 * (offset 9). Duas degeneracoes desligavam o evento em mapas grandes,
 * sem aviso:
 *   1. period == 0 a partir de ~L=3000, e due() tratava 0 como "nunca";
 *   2. period <= offset a partir de ~L=2000 (6 dias contra offset 9) —
 *      `day % 6` nunca vale 9, entao a congruencia nao tem solucao.
 * Este teste varre todo o intervalo aceito por Config (50..10000) e
 * exige que o evento dispare. As falhas vao para uma lista porque o
 * TCHECK do harness imprime a expressao, nao o valor do loop — a
 * lista vazia na saida diz exatamente quais lados quebraram.
 * Basta o primeiro disparo: um periodo saudavel sempre dispara no dia
 * `offset`, e as degeneracoes do BUG-26 nunca disparam. */
TTEST(dynamic_fire_never_silences_itself_on_large_maps) {
  const SimulationEvent* fire = findFire();
  if(!fire)
    return;

  std::string silenced;
  for(int mapLength : {50, 100, 200, 350, 500, 1000, 2000, 2683, 3000,
                       5000, 7500, 10000})
    if(firstDueDay(*fire, mapLength, 4000) < 0)
      silenced += " " + std::to_string(mapLength);
  TCHECK_EQ(silenced, std::string(""));
}

/* O conserto so pode mexer onde ja estava quebrado: nos mapas em que
 * period > offset a congruencia vale, e o intervalo entre disparos
 * consecutivos tem de ser exatamente o da formula legacy. */
TTEST(dynamic_fire_keeps_legacy_cadence_where_it_worked) {
  const SimulationEvent* fire = findFire();
  if(!fire)
    return;

  std::string wrongGap;
  for(int mapLength : {50, 100, 200, 350, 500, 1000}) {
    const int expected = DAYS_BETWEEN_FIRES * 100 / mapLength * 100
      / mapLength;
    // pre-condicao: a congruencia tem solucao, entao a regra nova nao
    // pode ter entrado por aqui
    TCHECK(expected > fire->offset);

    const int horizon = 2 * expected + 32;   // cabe dois disparos
    const int first = firstDueDay(*fire, mapLength, horizon);
    const int second = secondDueDay(*fire, mapLength, horizon);
    if(second < 0 || second - first != expected)
      wrongGap += " " + std::to_string(mapLength);
  }
  TCHECK_EQ(wrongGap, std::string(""));
}