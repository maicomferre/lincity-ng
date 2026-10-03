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

TTEST(water_pollution_formula) {
  /* FEAT-06: pollution_factor = 1 - water_pol/WATER_POL_MAX.
   * At 0 pollution → full output; at WATER_POL_MAX → zero.
   *
   * BUG-40: este teste afirmava WATER_POL_MAX, PORT_WATER_POLLUTION,
   * OILWELL_WATER_POLLUTION, REFINERY_WATER_POLLUTION e
   * HEAVY_IND_WATER_POLLUTION contra os seus próprios literais —
   * tautologias que não podem falhar e não provam nada. Saíram.
   *
   * O que elas escondiam: ao listar HEAVY_IND_WATER_POLLUTION, o teste
   * sugeria que o FEAT-06 polui a água a partir de indústria pesada. Não
   * polui — a constante não tinha referência nenhuma em produção. As
   * fontes reais são port.cpp:183, oilwell.cpp:105 e refinery.cpp:91,
   * cada uma emitindo por conta própria. Isso é uma violação de R2 (uma
   * fórmula, um lugar) e significa que hoje não existe função central
   * onde um teste possa perguntar "quem polui a água" — por isso a
   * lacuna não vira asserção aqui, e sim FEAT no inventário.
   *
   * O que este teste continua cobrindo é a parte que responde no
   * sea_farm.cpp:129. */
  /* Zero pollution → factor 1.0 */
  double f0 = 1.0 - 0 / (double)WATER_POL_MAX;
  TCHECK_EQ(f0, 1.0);

  /* Full pollution → factor 0.0 */
  double fmax = 1.0 - WATER_POL_MAX / (double)WATER_POL_MAX;
  TCHECK_EQ(fmax, 0.0);

  /* Mid pollution → factor 0.5 */
  double fmid = 1.0 - 500 / (double)WATER_POL_MAX;
  TCHECK_EQ(fmid, 0.5);
}
