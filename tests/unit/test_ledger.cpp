/* Layer 1 test: FEAT-04 per-construction ledger.
 *
 * Tests that recordExpense() and recordRevenue() accumulate
 * correctly on a Construction, and that purchase_cost /
 * capital_invested are set at build time via placeItem().
 */

#include "../tiny_test.hpp"

#include <memory>

#include "lincity/init_game.hpp"
#include "lincity/lintypes.hpp"
#include "lincity/world.hpp"

TTEST(ledger_records_expense_and_revenue) {
  city_settings city;
  city.with_village = true;
  std::unique_ptr<World> world = new_city(&city, 100,
    SimulationProtocol::DETERMINISTIC_V1, 0);
  TCHECK(world != nullptr);

  // Find any construction to test on
  Construction* cstr = nullptr;
  for(auto* c : world->map.constructions) {
    cstr = c;
    break;
  }
  TCHECK(cstr != nullptr);

  const int before_maint = cstr->maintenance;
  const int before_rev   = cstr->revenue;

  cstr->recordExpense(100);
  TCHECK_EQ(cstr->maintenance, before_maint + 100);

  cstr->recordExpense(50);
  TCHECK_EQ(cstr->maintenance, before_maint + 150);

  cstr->recordRevenue(200);
  TCHECK_EQ(cstr->revenue, before_rev + 200);

  cstr->recordRevenue(75);
  TCHECK_EQ(cstr->revenue, before_rev + 275);
}

TTEST(ledger_purchase_cost_set_at_build) {
  city_settings city;
  city.with_village = true;
  std::unique_ptr<World> world = new_city(&city, 100,
    SimulationProtocol::DETERMINISTIC_V1, 0);
  TCHECK(world != nullptr);

  // After new_city, there should be at least one construction
  TCHECK(!world->map.constructions.empty());

  for(const auto* cstr : world->map.constructions) {
    // Every construction should have purchase_cost == capital_invested
    TCHECK_EQ(cstr->purchase_cost, cstr->capital_invested);
    // purchase_cost should be non-negative
    TCHECK(cstr->purchase_cost >= 0);
  }
}

TTEST(ledger_maintenance_accumulates) {
  city_settings city;
  city.with_village = true;
  std::unique_ptr<World> world = new_city(&city, 100,
    SimulationProtocol::DETERMINISTIC_V1, 0);
  TCHECK(world != nullptr);

  Construction* cstr = nullptr;
  for(auto* c : world->map.constructions) {
    cstr = c; break;
  }
  TCHECK(cstr != nullptr);

  const int before = cstr->maintenance;
  cstr->recordExpense(100);
  cstr->recordExpense(250);
  cstr->recordExpense(75);
  TCHECK_EQ(cstr->maintenance, before + 425);
}

TTEST(ledger_revenue_accumulates) {
  city_settings city;
  city.with_village = true;
  std::unique_ptr<World> world = new_city(&city, 100,
    SimulationProtocol::DETERMINISTIC_V1, 0);
  TCHECK(world != nullptr);

  Construction* cstr = nullptr;
  for(auto* c : world->map.constructions) {
    cstr = c; break;
  }
  TCHECK(cstr != nullptr);

  const int before = cstr->revenue;
  cstr->recordRevenue(1000);
  cstr->recordRevenue(500);
  TCHECK_EQ(cstr->revenue, before + 1500);
}

TTEST(ledger_net_worth_is_revenue_minus_expenses) {
  city_settings city;
  city.with_village = true;
  std::unique_ptr<World> world = new_city(&city, 100,
    SimulationProtocol::DETERMINISTIC_V1, 0);
  TCHECK(world != nullptr);

  Construction* cstr = nullptr;
  for(auto* c : world->map.constructions) {
    cstr = c; break;
  }
  TCHECK(cstr != nullptr);

  cstr->recordExpense(200);
  cstr->recordExpense(300);
  cstr->recordRevenue(1000);
  cstr->recordRevenue(500);

  const int net_worth = cstr->revenue - (cstr->maintenance + cstr->operation_cost);
  TCHECK_EQ(net_worth, 1000);
  TCHECK(cstr->revenue > 0);
  TCHECK(cstr->maintenance > 0);
}
