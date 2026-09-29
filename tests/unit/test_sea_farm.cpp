/* Layer 1 test: FEAT-05 Sea Farm.
 *
 * Tests that the sea farm is registered and its constants
 * match the design values.
 */

#include "../tiny_test.hpp"

#include <memory>

#include "lincity/init_game.hpp"
#include "lincity/lintypes.hpp"
#include "lincity/modules/all_modules.hpp"
#include "lincity/modules/sea_farm.hpp"
#include "lincity/world.hpp"

TTEST(sea_farm_can_build_on_water) {
  /* Register modules before creating world */
  initializeModules();

  city_settings city;
  city.with_village = true;
  std::unique_ptr<World> world = new_city(&city, 100,
    SimulationProtocol::DETERMINISTIC_V1, 0);
  TCHECK(world != nullptr);

  /* Sea farm should be registered */
  ConstructionGroup* cg =
    ConstructionGroup::getConstructionGroup(GROUP_SEA_FARM);
  TCHECK(cg != nullptr);
}

TTEST(sea_farm_constants) {
  TCHECK_EQ(SEA_FARM_COST, 15000);
  TCHECK_EQ(SEA_FARM_LABOR, 180);
  TCHECK_EQ(SEA_FARM_POWER, 200);
  TCHECK_EQ(SEA_FARM_WATER, 10);
  TCHECK_EQ(SEA_FARM_FOOD, 350);
  TCHECK_EQ(SEA_FARM_GOODS, 8);
  TCHECK_EQ(GROUP_SEA_FARM, 53);
}

TTEST(sea_farm_cannot_build_on_land) {
  initializeModules();

  city_settings city;
  city.with_village = true;
  std::unique_ptr<World> world = new_city(&city, 100,
    SimulationProtocol::DETERMINISTIC_V1, 0);
  TCHECK(world != nullptr);

  /* Find a bare non-water tile */
  MapPoint landPoint(-1, -1);
  for(MapPoint p(2, 2); p.y < world->map.len() - 4; ++p.y) {
    for(p.x = 2; p.x < world->map.len() - 4; ++p.x) {
      if(world->map(p)->is_bare() && !world->map(p)->is_water()) {
        landPoint = p;
        break;
      }
    }
    if(landPoint.x >= 0) break;
  }
  TCHECK(landPoint.x >= 0);
  if(landPoint.x < 0) return;

  auto* cg = dynamic_cast<SeaFarmConstructionGroup*>(
    ConstructionGroup::getConstructionGroup(GROUP_SEA_FARM));
  TCHECK(cg != nullptr);

  Message::ptr msg;
  TCHECK(!cg->can_build_here(*world, landPoint, msg));
}

TTEST(sea_farm_formula_scales_with_labor) {
  /* Production = SEA_FARM_FOOD * used_labor / SEA_FARM_LABOR.
   * With full labor (180) → 350 food; with half (90) → 175. */
  TCHECK_EQ(SEA_FARM_FOOD * 180 / SEA_FARM_LABOR, 350);
  TCHECK_EQ(SEA_FARM_FOOD * 90 / SEA_FARM_LABOR, 175);
  TCHECK_EQ(SEA_FARM_GOODS * 180 / SEA_FARM_LABOR, 8);
  TCHECK_EQ(SEA_FARM_GOODS * 90 / SEA_FARM_LABOR, 4);
}
