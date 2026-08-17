/* simtest.cpp - headless simulation harness (test layer 2).
 *
 * Boots the engine without a real display (SDL dummy drivers), runs the
 * simulation for a fixed number of days with a fixed RNG seed, and checks
 * invariants:
 *   - money conservation: sum of money accounts matches the cash delta
 *   - commodity conservation: sum of all construction inventories matches
 *     the baseline plus the world commodity ledger (TST-03)
 *   - total_money stays within the engine clamp
 *   - stats.total_money stays in sync at month boundaries
 *   - scenario matrix: every data/opening/*.scn.gz loads and runs 1 year
 *   - reset flow: a fresh world after a simulated game starts clean
 *
 * Usage: lincity-test-sim [--app-data DIR] [--user-data DIR] [--days N]
 *                         [--seed N] [--scenario NAME]
 */

#include "../tiny_test.hpp"

#include <SDL3/SDL.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <tuple>
#include <stdexcept>
#include <thread>
#include <string>

#include "headless_env.hpp"
#include "gui/Paragraph.hpp"              // for Paragraph

#include "lincity-ng/Config.hpp"             // for getConfig
#include "lincity-ng/Game.hpp"               // for Game
#include "lincity-ng/GameView.hpp"           // for GameView
#include "lincity-ng/Dialog.hpp"            // for openGovernor, closeAllDialogs
#include "lincity-ng/MainLincity.hpp"        // for loadContinueCityNG
#include "lincity-ng/MapThumbnail.hpp"       // for MapThumbnail
#include "lincity-ng/MiniMap.hpp"            // for MiniMap
#include "lincity-ng/Util.hpp"              // for getParagraph
#include "lincity-ng/main.hpp"               // for initVideo, window
#include "lincity/all_buildings.hpp"          // for INCOME_TAX_RATE...
#include "lincity/commodities.hpp"           // for Commodity, STUFF_*
#include "lincity/groups.hpp"                // for GROUP_ROAD_BRIDGE
#include "lincity/messages.hpp"             // for Message
#include "lincity/modules/all_modules.hpp"   // for *ConstructionGroup
#include "lincity/modules/parkland.hpp"       // for parklandConstructionGroup
#include "lincity/modules/power_line.hpp"     // for powerlineConstructionGroup
#include "lincity/modules/residence.hpp"      // for residenceLLConstructionGroup
#include "lincity/modules/school.hpp"         // for schoolConstructionGroup
#include "lincity/modules/track_road_rail.hpp" // for roadConstructionGroup
#include "lincity/modules/waterwell.hpp"      // for waterwellConstructionGroup
#include "lincity/init_game.hpp"             // for city_settings, new_city
#include "lincity/lintypes.hpp"              // for Construction
#include "lincity/stats.hpp"                 // for Stats
#include "lincity/world.hpp"                 // for World
#include "util/randutil.hpp"                 // for BasicUrbg

namespace {

int g_days = 1200;            // one in-game year by default
uint64_t g_seed = 1234567;
std::string g_scenario;       // empty: create a random new city

/* base money at the start of the current in-game year (accounts were
 * finalized at that moment, so the accumulated accounts must add up to the
 * delta since then) */
long long g_year_base_money = 0;

/* base commodity inventories at the start of the current run (all ledger
 * deltas from before the run are folded in; the invariant covers only the
 * days simulated here) */
std::array<long long, STUFF_COUNT> g_commodity_base = {};

/* sum of every construction's inventory, the left side of the commodity
 * conservation invariant */
std::array<long long, STUFF_COUNT> commodity_totals(const World& world) {
  std::array<long long, STUFF_COUNT> totals = {};
  for(const Construction* cst : world.map.constructions)
    for(Commodity stuff = STUFF_INIT; stuff < STUFF_COUNT; stuff++)
      totals[stuff] += cst->commodityCount[stuff];
  return totals;
}

/* sum of the accumulating money accounts (income accumulates negatively) */
long long money_accounts_delta(const Stats& stats) {
  long long income = 0;
  income += stats.income.income_tax.acc;
  income += stats.income.coal_tax.acc;
  income += stats.income.ore_tax.acc;
  income += stats.income.goods_tax.acc;
  income += stats.income.export_tax.acc;

  long long expenses = 0;
  expenses += stats.expenses.construction.acc;
  expenses += stats.expenses.coalSurvey.acc;
  expenses += stats.expenses.import.acc;
  expenses += stats.expenses.unemployment.acc;
  expenses += stats.expenses.transport.acc;
  expenses += stats.expenses.windmill.acc;
  expenses += stats.expenses.university.acc;
  expenses += stats.expenses.recycle.acc;
  expenses += stats.expenses.deaths.acc;
  expenses += stats.expenses.health.acc;
  expenses += stats.expenses.rockets.acc;
  expenses += stats.expenses.school.acc;
  expenses += stats.expenses.firestation.acc;
  expenses += stats.expenses.cricket.acc;
  expenses += stats.expenses.interest.acc;

  return expenses + income;
}

std::unique_ptr<World> make_world() {
  // Reproducible worlds: every test starts from the same RNG state.
  BasicUrbg::get().reseed(g_seed);
  srand(g_seed);
  if(g_scenario.empty()) {
    city_settings city;
    city.with_village = true;
    city.without_trees = false;
    return new_city(&city, getConfig()->worldSize.get());
  }
  std::filesystem::path scenario = g_scenario;
  if(scenario.extension().empty())
    scenario += ".scn.gz";
  std::filesystem::path path =
    headless::app_data() / "opening" / scenario;
  return World::load(path);
}

/* Run the simulation like the Game loop does: one timestep per day and
 * periodic animations. Returns false on the first invariant violation.
 *
 * check_conservation can be disabled to smoke-test worlds whose account
 * balance is not expected to match the cash (A/B experiments).
 * check_commodities is the TST-03 goods invariant: the sum of all
 * construction inventories must equal the baseline plus the world's
 * commodity ledger (produce/consume/level/destruction deltas). */
bool run_days(World& world, int days, bool check_conservation = true,
  bool check_commodities = true) {
  Uint32 tick = 0;
  // Accounts were finalized at the last in-game year boundary; any
  // accruals that already exist (e.g. from build/bulldoze actions before
  // this run) are folded into the base so the invariant covers only the
  // days simulated here.
  g_year_base_money = world.total_money + money_accounts_delta(world.stats);
  g_commodity_base = commodity_totals(world);
  for(Commodity stuff = STUFF_INIT; stuff < STUFF_COUNT; stuff++)
    g_commodity_base[stuff] -= world.commodityLedger[stuff];
  for(int day = 0; day < days; day++) {
    // end_of_year_update() pays taxes on total_time % 1200 == 1199 and
    // Stats::yearly() finalizes the accounts on the next step
    // (total_time % 1200 == 0). Rebase right before the finalizing step so
    // the base covers everything accrued in the closing year.
    if((world.total_time + 1) % 1200 == 0)
      g_year_base_money = world.total_money;

    const long long money_before = world.total_money;
    const long long accounts_before = money_accounts_delta(world.stats);
    world.do_time_step();
    if(day % 30 == 0)
      world.do_animate(tick);
    tick += 100;

    if(std::llabs(world.total_money) > 2000000000LL) {
      std::fprintf(stderr, "  total_money %d escaped the engine clamp\n",
        world.total_money);
      return false;
    }

    // Money conservation: exact when the balance is far from the clamp,
    // where the yearly clamp/overflow adjustments cannot touch it.
    // Expense accounts accumulate positively, income accounts negatively
    // (World::income does account -= amt), so the expected balance is the
    // base minus the account sum.
    if(check_conservation
      && std::llabs(world.total_money) < 1000000000LL) {
      long long expected =
        g_year_base_money - money_accounts_delta(world.stats);
      if(expected != world.total_money) {
        std::fprintf(stderr,
          "  money conservation broken on day %d: expected %lld, got %d "
          "(base %lld, accounts %lld, this step: money %+lld accounts %+lld)\n",
          world.total_time, expected, world.total_money, g_year_base_money,
          money_accounts_delta(world.stats),
          (long long)world.total_money - money_before,
          money_accounts_delta(world.stats) - accounts_before);
        const auto& s = world.stats;
        std::fprintf(stderr,
          "    income: tax %d coal %d ore %d goods %d export %d\n",
          s.income.income_tax.acc, s.income.coal_tax.acc,
          s.income.ore_tax.acc, s.income.goods_tax.acc,
          s.income.export_tax.acc);
        std::fprintf(stderr,
          "    expense: cst %d survey %d import %d unemp %d transp %d "
          "windmill %d uni %d recycle %d deaths %d health %d rocket %d "
          "school %d fire %d cricket %d interest %d\n",
          s.expenses.construction.acc, s.expenses.coalSurvey.acc,
          s.expenses.import.acc, s.expenses.unemployment.acc,
          s.expenses.transport.acc, s.expenses.windmill.acc,
          s.expenses.university.acc, s.expenses.recycle.acc,
          s.expenses.deaths.acc, s.expenses.health.acc,
          s.expenses.rockets.acc, s.expenses.school.acc,
          s.expenses.firestation.acc, s.expenses.cricket.acc,
          s.expenses.interest.acc);
        return false;
      }
    }

    // stats.total_money is synced at the end of each month (total_time
    // % 100 == 99). At year ends the sync runs before end_of_year_update()
    // pays the taxes, so skip the sync check there (known quirk).
    if(world.total_time % 100 == 99
      && world.total_time % 1200 != 1199
      && world.stats.total_money != world.total_money) {
      std::fprintf(stderr,
        "  stats.total_money out of sync on day %d: %d vs %d\n",
        world.total_time, world.stats.total_money, world.total_money);
      return false;
    }

    // TST-03: commodity conservation. Every inventory change goes through
    // produceStuff/consumeStuff/levelStuff or the Construction destructor,
    // all of which update world.commodityLedger, so the sum of inventories
    // must track the baseline plus the ledger exactly.
    if(check_commodities) {
      const auto totals = commodity_totals(world);
      for(Commodity stuff = STUFF_INIT; stuff < STUFF_COUNT; stuff++) {
        const long long expected =
          g_commodity_base[stuff] + world.commodityLedger[stuff];
        if(totals[stuff] != expected) {
          std::fprintf(stderr,
            "  commodity conservation broken on day %d: %s totals %lld, "
            "expected %lld (base %lld, ledger %lld)\n",
            world.total_time, commodityStandardName(stuff), totals[stuff],
            expected, g_commodity_base[stuff], world.commodityLedger[stuff]);
          return false;
        }
      }
    }
  }
  return true;
}

TTEST(world_boots_and_runs_one_year) {
  std::unique_ptr<World> world = make_world();
  if(!world) {
    TCHECK(!"failed to create world");
    return;
  }
  TCHECK(world->total_money > -2000000000);
  TCHECK(world->total_money < 2000000000);

  TCHECK(run_days(*world, g_days));

  // stats.total_money is synced at month ends (total_time % 100 == 99)
  if(world->total_time % 100 == 99)
    TCHECK_EQ(world->stats.total_money, world->total_money);
  TCHECK(world->total_money >= -2000000000);
  TCHECK(world->total_money <= 2000000000);
}

TTEST(money_conservation_one_year) {
  std::unique_ptr<World> world = make_world();
  if(!world) {
    TCHECK(!"failed to create world");
    return;
  }
  TCHECK(run_days(*world, g_days));
}

TTEST(credit_limit_keeps_simulating) {
  // BUG-11: at the credit limit every expense() throws
  // OutOfMoneyMessage::Exception. All buildings catch it except Windmill,
  // which caught the message instead of the exception, letting it escape
  // the simulation and bounce the game back to the main menu.
  std::unique_ptr<World> world = make_world();
  if(!world) {
    TCHECK(!"failed to create world");
    return;
  }
  world->tech_level = 60000;

  // place a windmill so Windmill::update() runs each day
  MapPoint p(5, 5);
  try {
    world->buildConstruction(windmillConstructionGroup, p);
  } catch(const Message::Exception& err) {
    std::fprintf(stderr, "  windmill build failed: %s\n", err.what());
    TCHECK(!"windmill build threw");
    return;
  }
  TCHECK(world->map(p)->construction != nullptr);

  // force the city to the credit limit (test-only direct write)
  world->total_money = -2000000000;

  // the simulation must keep stepping without throwing
  for(int day = 0; day < 1200; day++) {
    try {
      world->do_time_step();
    } catch(const Message::Exception& err) {
      std::fprintf(stderr, "  simulation threw on day %d: %s\n",
        world->total_time, err.what());
      TCHECK(!"simulation threw at the credit limit");
      return;
    }
  }
}

TTEST(scenario_matrix_one_year_each) {
  int loaded = 0;
  for(const auto& entry :
    std::filesystem::directory_iterator(headless::app_data() / "opening")
  ) {
    if(!entry.is_regular_file())
      continue;
    if(entry.path().extension() != ".gz")
      continue;
    if(entry.path().filename().string().rfind(".scn.gz") == std::string::npos)
      continue;

    std::unique_ptr<World> world;
    try {
      world = World::load(entry.path());
    } catch(const std::exception& err) {
      std::fprintf(stderr, "  failed to load %s: %s\n",
        entry.path().string().c_str(), err.what());
      TCHECK(!"scenario failed to load");
      continue;
    }
    loaded++;
    TCHECK(run_days(*world, 1200));
  }
  TCHECK(loaded >= 6);
}

TTEST(reset_flow_new_world_after_game) {
  // Play a world, then create a fresh one and make sure it is clean.
  std::unique_ptr<World> first = make_world();
  if(!first) {
    TCHECK(!"failed to create first world");
    return;
  }
  TCHECK(run_days(*first, 300));
  TCHECK_EQ(first->total_time, 300);

  std::unique_ptr<World> second = make_world();
  if(!second) {
    TCHECK(!"failed to create second world");
    return;
  }
  // A new world starts at day 0 with fresh counters.
  TCHECK_EQ(second->total_time, 0);
  TCHECK(run_days(*second, 300));
  TCHECK_EQ(second->total_time, 300);
}

TTEST(game_flow_reset_and_loading_barrier) {
  // GUI flow regression for BUG-01a + BUG-02 with a single Game object
  // (the real menu reuses its Game across games).
  initVideo(800, 600);
  Game game(window);

  // BUG-01a: installing a world must redraw both map views and re-enable
  // autosave.
  std::unique_ptr<World> first = make_world();
  if(!first) {
    TCHECK(!"failed to create first world");
    return;
  }
  TCHECK(run_days(*first, 300));
  game.setWorld(std::move(first));

  TCHECK(game.getGameView().isMapDirty());
  TCHECK(game.getMiniMap().isMapDirty());
  TCHECK_EQ(game.getLastAutosaveDay(), -1);
  TCHECK_EQ((long long)game.getLastAutosaveTick(), 0LL);

  // A second consecutive new game resets again (the reported flow:
  // play -> menu -> new game).
  std::unique_ptr<World> second = make_world();
  if(!second) {
    TCHECK(!"failed to create second world");
    return;
  }
  TCHECK(run_days(*second, 200));
  game.setWorld(std::move(second));
  TCHECK(game.getGameView().isMapDirty());
  TCHECK(game.getMiniMap().isMapDirty());
  TCHECK_EQ(game.getLastAutosaveDay(), -1);

  // FEAT-03c: the governor dialog opens and shows the current rates
  openGovernor(game);
  Paragraph* gov0 = getParagraph(game.getGui(), "govValue0");
  TCHECK(gov0 != nullptr);
  if(gov0)
    TCHECK_EQ(gov0->getText(), std::string("8")); // INCOME_TAX_RATE
  closeAllDialogs();

  // BUG-02: Game::run must not enter the main loop before the image
  // loader finished and every image became a texture. Send QUIT from a
  // helper thread so run() leaves the main loop by itself (same path the
  // real game uses on window close).
  std::thread quitter([] {
    SDL_Delay(5000);
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&event);
  });
  game.run();
  quitter.join();

  // When run() returns the whole loading pipeline must be complete.
  TCHECK(game.getGameView().textures_ready);
  TCHECK_EQ((int)game.getGameView().remaining_images, 0);
}

TTEST(continue_slot_chain_loads_in_order) {
  // BUG-01b: Continue must read 9_currentGameNG.scn.gz first, fall back
  // to autosave.scn.gz, and create a fresh city when neither exists.
  const auto userData = headless::user_data();
  const auto currentPath = userData / "9_currentGameNG.scn.gz";
  const auto autosavePath = userData / "autosave.scn.gz";
  std::error_code ec;
  std::filesystem::remove(currentPath, ec);
  std::filesystem::remove(autosavePath, ec);

  city_settings city;
  city.with_village = true;
  city.without_trees = false;

  // Marker worlds with distinguishable total_time.
  std::unique_ptr<World> worldA = new_city(&city, 100);
  worldA->do_time_step(); // total_time = 1
  std::unique_ptr<World> worldB = new_city(&city, 100);
  worldB->do_time_step();
  worldB->do_time_step(); // total_time = 2
  worldA->save(currentPath);
  worldB->save(autosavePath);

  // 1) both slots present -> the clean-exit save wins
  std::unique_ptr<World> r1 = loadContinueCityNG(userData, 100);
  TCHECK(r1 != nullptr);
  if(r1)
    TCHECK_EQ(r1->total_time, 1);

  // 2) only the autosave remains -> autosave is loaded
  std::filesystem::remove(currentPath, ec);
  std::unique_ptr<World> r2 = loadContinueCityNG(userData, 100);
  TCHECK(r2 != nullptr);
  if(r2)
    TCHECK_EQ(r2->total_time, 2);

  // 3) nothing left -> fresh city at day 0
  std::filesystem::remove(autosavePath, ec);
  std::unique_ptr<World> r3 = loadContinueCityNG(userData, 100);
  TCHECK(r3 != nullptr);
  if(r3)
    TCHECK_EQ(r3->total_time, 0);
}

TTEST(bridge_charges_resolved_cost) {
  // BUG-04: a road segment placed on water becomes a bridge and must be
  // charged the bridge price (BRIDGE_FACTOR x the road), not the road
  // price that was displayed before.
  std::unique_ptr<World> world = make_world();
  if(!world) {
    TCHECK(!"failed to create world");
    return;
  }

  MapPoint waterPoint(-1, -1);
  for(int y = 0; y < world->map.len() && waterPoint.x < 0; y++)
    for(int x = 0; x < world->map.len(); x++) {
      if(world->map(MapPoint(x, y))->is_water()) {
        waterPoint = MapPoint(x, y);
        break;
      }
    }
  if(waterPoint.x < 0) {
    TCHECK(!"no water tile found in new city");
    return;
  }

  // roads unlock at tech 50000; use a high-tech city for the test
  // (test-only direct write, worlds are throwaway here)
  world->tech_level = 60000;

  const int moneyBefore = world->total_money;
  try {
    world->buildConstruction(roadConstructionGroup, waterPoint);
  } catch(const Message::Exception& err) {
    std::fprintf(stderr, "  buildConstruction failed: %s\n", err.what());
    TCHECK(!"bridge build threw");
    return;
  } catch(const std::exception& err) {
    std::fprintf(stderr, "  buildConstruction failed: %s\n", err.what());
    TCHECK(!"bridge build threw");
    return;
  }
  const int charged = moneyBefore - world->total_money;
  const int roadCost = roadConstructionGroup.getCosts(*world);
  const int bridgeCost = roadbridgeConstructionGroup.getCosts(*world);

  TCHECK(charged != roadCost);
  TCHECK_EQ(charged, bridgeCost);

  // the tile must now carry a road bridge (getTransportGroup reports
  // bridges as their normal transport group, so check the raw group)
  TCHECK(world->map(waterPoint)->getGroup() == GROUP_ROAD_BRIDGE);
}

TTEST(scenario_thumbnail_and_panel_data) {
  // FEAT-02 pipeline: load a scenario, generate its thumbnail texture and
  // collect the panel data the menu shows for it.
  initVideo(800, 600);

  std::unique_ptr<World> world = World::load(
    headless::app_data() / "opening" / "good_times.scn.gz");
  TCHECK(world != nullptr);
  if(!world)
    return;

  MapThumbnail thumbnail;
  thumbnail.resize(160, 160);
  thumbnail.setWorld(world.get());
  // drawing once exercises the texture and the color mapping
  thumbnail.draw(*painter);

  // panel data sanity (what mapSelectionChanged formats); scenarios
  // predate the persisted population stats, so they are 0 after load
  TCHECK(world->total_money > 0);
  TCHECK(world->map.len() > 0);
  TCHECK(world->tech_level > 0);
}

TTEST(tax_elasticity_ab_reduces_activity) {
  // FEAT-03c stage 2, A/B with the same seed on the same scenario:
  // doubling every rate must not increase industry pollution or
  // population (elasticity only reduces taxable flows), and the money
  // conservation invariant must hold in both runs.
  auto runScenario = [](double factor) {
    BasicUrbg::get().reseed(g_seed);
    srand(g_seed);
    std::unique_ptr<World> w = World::load(
      headless::app_data() / "opening" / "good_times.scn.gz");
    if(!w)
      return std::tuple<bool, int, int>{false, 0, 0};
    // test-only direct writes of the tax rates
    w->money_rates.income_tax = (int)(INCOME_TAX_RATE * factor);
    w->money_rates.coal_tax = (int)(COAL_TAX_RATE * factor);
    w->money_rates.ore_tax = (int)(ORE_TAX_RATE * factor);
    w->money_rates.goods_tax = (int)(GOODS_TAX_RATE * factor);
    w->money_rates.dole = (int)(DOLE_RATE * factor);
    w->money_rates.transport_cost = (int)(TRANSPORT_COST_RATE * factor);
    const bool ok = run_days(*w, 1200);
    return std::tuple<bool, int, int>{ok,
      w->stats.total_pollution,
      w->stats.population.population_m.acc};
  };

  auto base = runScenario(1.0);
  auto high = runScenario(2.0);
  std::fprintf(stderr, "  base: pollution %d pop %d | high: pollution %d pop %d\n",
    std::get<1>(base), std::get<2>(base),
    std::get<1>(high), std::get<2>(high));

  TCHECK(std::get<0>(base));
  TCHECK(std::get<0>(high));
  TCHECK(std::get<1>(high) <= std::get<1>(base));
  TCHECK(std::get<2>(high) <= std::get<2>(base));
}


TTEST(random_actions_keep_conservation) {
  // TST-03: random build/demolish/evacuate actions with a fixed seed;
  // the money conservation invariant must hold through all of them.
  std::unique_ptr<World> world = make_world();
  if(!world) {
    TCHECK(!"failed to create world");
    return;
  }
  world->tech_level = 60000; // everything unlockable for the stress test

  // candidate constructions to spam the city with
  std::vector<ConstructionGroup*> groups = {
    &roadConstructionGroup,
    &trackConstructionGroup,
    &railConstructionGroup,
    &powerlineConstructionGroup,
    &windmillConstructionGroup,
    &residenceLLConstructionGroup,
    &marketConstructionGroup,
    &waterwellConstructionGroup,
    &organic_farmConstructionGroup,
    &parklandConstructionGroup,
    &schoolConstructionGroup,
  };

  const int len = world->map.len();
  for(int action = 0; action < 500; action++) {
    if(rand() % 2) {
      // random build attempt somewhere on the map
      ConstructionGroup& group =
        *groups[rand() % groups.size()];
      MapPoint point(1 + rand() % (len - 2), 1 + rand() % (len - 2));
      try {
        world->buildConstruction(group, point);
      } catch(const Message::Exception&) {
        // invalid placement is expected for random points
      }
    }
    else {
      // random demolition of an existing construction
      if(!world->map.constructions.empty()) {
        auto it = world->map.constructions.begin();
        std::advance(it, rand() % world->map.constructions.size());
        try {
          world->bulldozeArea((*it)->point);
        } catch(const Message::Exception&) {
          // e.g. something not bulldozable
        }
      }
    }
    if(action % 5 == 0)
      world->do_time_step();
  }

  // the city must still obey the invariant afterwards
  TCHECK(run_days(*world, 300));
}

} // namespace

int main(int argc, char** argv) {
  std::filesystem::path appData = LINCITYNG_TEST_DATA_DIR;
  std::filesystem::path userData = std::filesystem::temp_directory_path()
    / "lincity-ng-test-sim";

  for(int argi = 1; argi < argc; argi++) {
    const std::string arg = argv[argi];
    auto next = [&]() -> std::string {
      if(argi + 1 >= argc)
        throw std::runtime_error(arg + " needs a parameter");
      return argv[++argi];
    };
    if(arg == "--app-data")
      appData = next();
    else if(arg == "--user-data")
      userData = next();
    else if(arg == "--days")
      g_days = std::stoi(next());
    else if(arg == "--seed")
      g_seed = std::stoull(next());
    else if(arg == "--scenario")
      g_scenario = next();
    else
      throw std::runtime_error("unrecognized argument: " + arg);
  }

  int rc;
  try {
    headless::init_env(appData, userData, g_seed);
    rc = tiny_test::run_all_tests();
    headless::shutdown_env();
  } catch(const std::exception& err) {
    std::fprintf(stderr, "simtest bootstrap failed: %s\n", err.what());
    return 1;
  }
  return rc;
}
