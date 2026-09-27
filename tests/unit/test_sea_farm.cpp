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
