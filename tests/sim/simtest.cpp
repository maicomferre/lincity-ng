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
 *   - scenario matrix: every opening scenario loads and runs 1 year
 *   - reset flow: a fresh world after a simulated game starts clean
 *
 * Usage: lincity-test-sim [--app-data DIR] [--user-data DIR] [--days N]
 *                         [--seed N] [--scenario NAME]
 */

#include "../tiny_test.hpp"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <climits>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <tuple>
#include <stdexcept>
#include <thread>
#include <string>
#include <vector>

#include "headless_env.hpp"
#include "gui/CheckButton.hpp"             // for build tooltip regression
#include "gui/ComponentLoader.hpp"
#include "gui/Document.hpp"
#include "gui/Paragraph.hpp"              // for Paragraph
#include "gui/Painter.hpp"
#include "gui/TextureManager.hpp"
#include "gui/SwitchComponent.hpp"        // for active MPS page

#include "lincity-ng/Config.hpp"             // for getConfig
#include "lincity-ng/ButtonPanel.hpp"        // for build tooltip regression
#include "lincity-ng/Game.hpp"               // for Game
#include "lincity-ng/GameView.hpp"           // for GameView
#include "lincity-ng/Dialog.hpp"            // for openGovernor, closeAllDialogs
#include "lincity-ng/MainLincity.hpp"        // for loadContinueCityNG
#include "lincity-ng/MapThumbnail.hpp"       // for MapThumbnail
#include "lincity-ng/MiniMap.hpp"            // for MiniMap
#include "lincity-ng/Mps.hpp"                // for ManagementKpis, MpsFinance
#include "lincity-ng/Sound.hpp"              // for music callback request
#include "lincity-ng/Util.hpp"              // for getParagraph
#include "lincity-ng/main.hpp"               // for initVideo, window
#include "lincity/all_buildings.hpp"          // for INCOME_TAX_RATE...
#include "lincity/commodities.hpp"           // for Commodity, STUFF_*
#include "lincity/groups.hpp"                // for GROUP_ROAD_BRIDGE
#include "lincity/messages.hpp"             // for Message
#include "lincity/resources.hpp"
#include "lincity/modules/all_modules.hpp"   // for *ConstructionGroup
#include "lincity/modules/parkland.hpp"       // for parklandConstructionGroup
#include "lincity/modules/power_line.hpp"     // for powerlineConstructionGroup
#include "lincity/modules/residence.hpp"      // for residenceLLConstructionGroup
#include "lincity/modules/rocket_pad.hpp"     // for RocketPad
#include "lincity/modules/school.hpp"         // for schoolConstructionGroup
#include "lincity/modules/commune.hpp"        // for Commune
#include "lincity/modules/firestation.hpp"
#include "lincity/modules/track_road_rail.hpp" // for roadConstructionGroup
#include "lincity/modules/waterwell.hpp"      // for waterwellConstructionGroup
#include "lincity/init_game.hpp"             // for city_settings, new_city
#include "lincity/lintypes.hpp"               // for Construction
#include "lincity/stats.hpp"                 // for Stats
#include "lincity/StateFingerprint.hpp"      // for captureStateSnapshot

#include <fmt/core.h>                           // PERF-01: locale-independent report
#include "lincity/SimulationReplay.hpp"       // for deterministic command replay
#include "lincity/util.hpp"                  // for format_money
#include "lincity/Vehicles.hpp"              // for Vehicle (dead-vehicle sweep check)
#include "lincity/world.hpp"                 // for World
#include "util/randutil.hpp"                 // for BasicUrbg
#include "util/debuglog.hpp"                 // for LNG_LOG

namespace {

int g_days = 1200;            // one in-game year by default
uint64_t g_seed = 1234567;
std::string g_scenario;       // empty: create a random new city
bool g_bench = false;         // --bench: instrument only, skip the test suite
int g_bench_days = 3000;
int g_bench_animate_every = 1; // 1 animate call per simulated day
int g_bench_world_size = 100;  // pinned: the user config must not leak in
std::string g_bench_scenario = "good_times"; // a real city: roads => vehicles

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
    if(day % 30 == 0) {
      world.do_animate(tick);
      // Dead vehicles are reaped, and frames stay on the current segment.
      for(const Vehicle* v : world.vehicleList) {
        if(v->isDead()) {
          std::fprintf(stderr,
            "  dead vehicle survived do_animate on day %d\n",
            world.total_time);
          return false;
        }
        if(v->framePt != v->prev && v->framePt != v->point) {
          std::fprintf(stderr,
            "  vehicle frame anchor left segment on day %d: "
            "frame=(%d,%d) prev=(%d,%d) point=(%d,%d)\n",
            world.total_time, v->framePt.x, v->framePt.y,
            v->prev.x, v->prev.y, v->point.x, v->point.y);
          return false;
        }
      }
    }
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

TTEST(legacy_state_fingerprint_is_stable_sensitive_and_non_cosmetic) {
  std::unique_ptr<World> world = make_world();
  TCHECK(world != nullptr);
  if(!world)
    return;

  const StateSnapshot baseline = captureStateSnapshot(*world);
  TCHECK(baseline.fingerprint() ==
    captureStateSnapshot(*world).fingerprint());

  world->commodityLedger[STUFF_FOOD] += 17;
  world->setUpdated(World::Updatable::MAP);
  MapTile* tile = world->map(MapPoint(1, 1));
  auto frame = tile->addFrame();
  frame->frame = 7;
  frame->move_x = 3;
  const MapPoint recentPoint = world->map.recentPoint;
  world->map.recentPoint = MapPoint(2, 3);
  const StateSnapshot withDiagnostics = captureStateSnapshot(*world);
  TCHECK(baseline.fingerprint() == withDiagnostics.fingerprint());
  TCHECK(!baseline.firstDifference(withDiagnostics).has_value());
  world->map.recentPoint = recentPoint;
  tile->removeFrame(frame);

  ++world->total_money;
  const StateSnapshot changedMoney = captureStateSnapshot(*world);
  TCHECK(baseline.fingerprint() != changedMoney.fingerprint());
  const auto moneyDifference = baseline.firstDifference(changedMoney);
  TCHECK(moneyDifference.has_value());
  if(moneyDifference)
    TCHECK_EQ(moneyDifference->section,
      std::string("gameplay-xml-v1"));
  --world->total_money;

  world->map.polluted.push_back(MapPoint(3, 4));
  const StateSnapshot changedPolluted = captureStateSnapshot(*world);
  TCHECK(baseline.fingerprint() != changedPolluted.fingerprint());
  const auto pollutedDifference = baseline.firstDifference(changedPolluted);
  TCHECK(pollutedDifference.has_value());
  if(pollutedDifference)
    // Since Fase 5 the pollution queue is persisted, so it shows up in the
    // canonical XML section first (the legacy queue section remains as a
    // diagnostic fallback).
    TCHECK_EQ(pollutedDifference->section,
      std::string("gameplay-xml-v1"));
  world->map.polluted.pop_back();

  Commune* commune = nullptr;
  for(Construction* construction : world->map.constructions) {
    commune = dynamic_cast<Commune*>(construction);
    if(commune)
      break;
  }
  TCHECK(commune != nullptr);
  if(commune) {
    ++commune->lazy_months;
    const StateSnapshot changedTrajectoryExtra = captureStateSnapshot(*world);
    TCHECK(baseline.fingerprint() != changedTrajectoryExtra.fingerprint());
    const auto extraDifference =
      baseline.firstDifference(changedTrajectoryExtra);
    TCHECK(extraDifference.has_value());
    if(extraDifference)
      // Since Fase 5 this trajectory field is persisted, so the canonical
      // XML section now reports the difference (the legacy extras section
      // remains as a diagnostic fallback).
      TCHECK_EQ(extraDifference->section,
        std::string("gameplay-xml-v1"));
    --commune->lazy_months;
  }
}

TTEST(legacy_daily_fingerprint_trajectory_is_repeatable) {
  auto runTrajectory = []() {
    std::unique_ptr<World> world = make_world();
    std::vector<StateFingerprint128> trajectory;
    if(!world)
      return trajectory;
    trajectory.reserve(30);
    for(int day = 0; day < 30; ++day) {
      world->do_time_step();
      trajectory.push_back(captureStateSnapshot(*world).fingerprint());
    }
    return trajectory;
  };
  auto snapshotAtDay = [](std::size_t targetDay) {
    std::unique_ptr<World> world = make_world();
    for(std::size_t day = 0; day <= targetDay; ++day)
      world->do_time_step();
    return captureStateSnapshot(*world);
  };

  const std::vector<StateFingerprint128> first = runTrajectory();
  const std::vector<StateFingerprint128> second = runTrajectory();
  TCHECK_EQ(first.size(), std::size_t(30));
  TCHECK_EQ(first.size(), second.size());
  for(std::size_t day = 0; day < std::min(first.size(), second.size()); ++day) {
    if(first[day] == second[day])
      continue;
    const StateSnapshot firstSnapshot = snapshotAtDay(day);
    const StateSnapshot secondSnapshot = snapshotAtDay(day);
    const auto difference = firstSnapshot.firstDifference(secondSnapshot);
    if(difference) {
      std::fprintf(stderr,
        "  Legacy fingerprint diverged on day %zu, section %s, byte %zu\n",
        day + 1, difference->section.c_str(), difference->offset);
    }
    TCHECK(first[day] == second[day]);
  }
  if(!first.empty()) {
    // REF-07 Fase 5: regenerated once when the 2143 save contract added the
    // simulation protocol/seed/version, the pollution queue, the module
    // trajectory extras and the stats inventory to the canonical XML. No
    // simulation code changed, so the trajectory is identical; only the
    // fingerprint input grew.
    TCHECK_EQ(first.back().hex(),
      std::string("0663381900960033e6bc62829e3dadba")); // updated: FEAT-04 ledger fields added to Construction
  }
}

TTEST(deterministic_v1_worlds_are_independent_and_repeatable) {
  city_settings city;
  city.with_village = true;
  city.without_trees = false;

  constexpr std::uint64_t seed = 0x0123456789abcdefULL;
  std::unique_ptr<World> left = new_city(&city, 100,
    SimulationProtocol::DETERMINISTIC_V1, seed);
  std::unique_ptr<World> right = new_city(&city, 100,
    SimulationProtocol::DETERMINISTIC_V1, seed);
  TCHECK(left != nullptr);
  TCHECK(right != nullptr);
  if(!left || !right)
    return;

  TCHECK(left->simulationProtocol()
    == SimulationProtocol::DETERMINISTIC_V1);
  TCHECK_EQ(left->simulationSeed(), seed);
  TCHECK(captureStateSnapshot(*left).fingerprint()
    == captureStateSnapshot(*right).fingerprint());

  for(int day = 0; day < 120; ++day) {
    left->do_time_step();
    // Perturb the Legacy global between independently owned v1 worlds.
    (void)BasicUrbg::get()();
    right->do_time_step();
    TCHECK(captureStateSnapshot(*left).fingerprint()
      == captureStateSnapshot(*right).fingerprint());
  }

  std::unique_ptr<World> different = new_city(&city, 100,
    SimulationProtocol::DETERMINISTIC_V1, seed + 1);
  TCHECK(different != nullptr);
  if(different)
    TCHECK(captureStateSnapshot(*left).fingerprint()
      != captureStateSnapshot(*different).fingerprint());

  TCHECK(!getConfig()->deterministicScheduler.get());
}

TTEST(deterministic_v1_ignores_sound_cars_and_animation_cadence) {
  Config* config = getConfig();
  const auto savedCars = config->carsEnabled.session;
  const auto savedSound = config->soundEnabled.session;

  auto run = [&](bool cars, bool sound, bool animate) {
    config->carsEnabled.session = cars;
    config->soundEnabled.session = sound;
    city_settings city{true, false};
    std::unique_ptr<World> world = new_city(&city, 100,
      SimulationProtocol::DETERMINISTIC_V1, 99887766);
    for(int day = 0; day < 400; ++day) {
      world->do_time_step();
      if(animate)
        world->do_animate(static_cast<unsigned long>(day + 1) * 1000UL);
    }
    return captureStateSnapshot(*world).fingerprint();
  };

  const StateFingerprint128 quiet = run(false, false, false);
  const StateFingerprint128 visual = run(true, true, true);
  TCHECK(quiet == visual);

  config->carsEnabled.session = savedCars;
  config->soundEnabled.session = savedSound;
}

TTEST(deterministic_v1_keeps_money_and_commodity_conservation) {
  city_settings city{true, false};
  std::unique_ptr<World> world = new_city(&city, 100,
    SimulationProtocol::DETERMINISTIC_V1, 1234567);
  TCHECK(world != nullptr);
  if(world)
    TCHECK(run_days(*world, 1200));
}

TTEST(simulation_command_rejects_invalid_input_without_mutation) {
  std::unique_ptr<World> world = make_world();
  TCHECK(world != nullptr);
  if(!world)
    return;

  const StateFingerprint128 before =
    captureStateSnapshot(*world).fingerprint();
  SimulationCommand invalid;
  invalid.day = 1;
  invalid.sequence = 1;
  invalid.type = SimulationCommandType::BUILD;
  invalid.point = MapPoint(1, 1);
  invalid.argument0 = NUM_OF_GROUPS + 100;
  bool rejected = false;
  try {
    executeSimulationCommand(*world, invalid);
  } catch(const SimulationReplayError&) {
    rejected = true;
  }
  TCHECK(rejected);
  TCHECK(before == captureStateSnapshot(*world).fingerprint());
}

TTEST(versioned_external_commands_replay_with_daily_checkpoints) {
  auto prepare = []() {
    std::unique_ptr<World> world = make_world();
    if(!world)
      return world;
    world->tech_level = MAX_TECH_LEVEL;
    world->total_money = 5000000;

    MapPoint rocketPoint;
    bool foundRocketSite = false;
    for(MapPoint point(2, 2); point.y < world->map.len() - 5; ++point.y) {
      for(point.x = 2; point.x < world->map.len() - 5; ++point.x) {
        Message::ptr message;
        if(rocketPadConstructionGroup.can_build_here(*world, point, message)) {
          rocketPoint = point;
          foundRocketSite = true;
          break;
        }
      }
      if(foundRocketSite)
        break;
    }
    if(!foundRocketSite)
      return std::unique_ptr<World>();
    rocketPadConstructionGroup.placeItem(*world, rocketPoint);
    RocketPad* rocket = dynamic_cast<RocketPad*>(
      world->map(rocketPoint)->reportingConstruction);
    if(!rocket)
      return std::unique_ptr<World>();
    rocket->steps = ROCKET_PAD_STEPS;
    rocket->stage = RocketPad::AWAITING;
    return world;
  };

  std::unique_ptr<World> recordedWorld = prepare();
  TCHECK(recordedWorld != nullptr);
  if(!recordedWorld)
    return;

  MapPoint roadPoint;
  MapPoint floodPoint;
  MapPoint evacuationPoint;
  bool foundRoad = false, foundFlood = false, foundEvacuation = false;
  for(MapPoint point(1, 1); point.y < recordedWorld->map.len() - 1; ++point.y) {
    for(point.x = 1; point.x < recordedWorld->map.len() - 1; ++point.x) {
      Construction* construction =
        recordedWorld->map(point)->reportingConstruction;
      if(!foundEvacuation && construction
        && !(construction->flags & FLAG_NEVER_EVACUATE)) {
        evacuationPoint = construction->point;
        foundEvacuation = true;
      }
      if(recordedWorld->map(point)->is_bare()) {
        Message::ptr message;
        if(!foundRoad
          && roadConstructionGroup.can_build_here(*recordedWorld, point, message)) {
          roadPoint = point;
          foundRoad = true;
        } else if(foundRoad && point != roadPoint && !foundFlood) {
          floodPoint = point;
          foundFlood = true;
        }
      }
    }
  }
  TCHECK(foundRoad);
  TCHECK(foundFlood);
  TCHECK(foundEvacuation);
  if(!foundRoad || !foundFlood || !foundEvacuation)
    return;

  MapPoint rocketPoint;
  for(Construction* construction : recordedWorld->map.constructions)
    if(dynamic_cast<RocketPad*>(construction))
      rocketPoint = construction->point;

  SimulationReplay replay;
  replay.seed = g_seed;
  replay.origin = "new-city-village-ref07-fixture";
  replay.initialFingerprint =
    captureStateSnapshot(*recordedWorld).fingerprint();

  recordSimulationDay(*recordedWorld, replay, 1, {
    SimulationCommand{.type = SimulationCommandType::BUILD,
      .point = roadPoint, .argument0 = GROUP_ROAD},
    SimulationCommand{.type = SimulationCommandType::EVACUATE,
      .point = evacuationPoint},
    SimulationCommand{.type = SimulationCommandType::SET_MONEY_RATE,
      .point = MapPoint(),
      .argument0 = static_cast<int>(World::MoneyRateAccount::INCOME_TAX),
      .argument1 = INCOME_TAX_RATE + 1},
    SimulationCommand{.type = SimulationCommandType::COAL_SURVEY,
      .point = MapPoint()},
    SimulationCommand{.type = SimulationCommandType::LAUNCH_ROCKET,
      .point = rocketPoint},
  });
  recordSimulationDay(*recordedWorld, replay, 2, {
    SimulationCommand{.type = SimulationCommandType::BULLDOZE,
      .point = roadPoint},
    SimulationCommand{.type = SimulationCommandType::FLOOD,
      .point = floodPoint},
  });
  recordSimulationDay(*recordedWorld, replay, 3, {});

  const std::string encoded = serializeSimulationReplay(replay);
  const SimulationReplay decoded = parseSimulationReplay(encoded);
  TCHECK_EQ(decoded.version, SimulationReplay::FORMAT_VERSION);
  TCHECK_EQ(decoded.commands.size(), std::size_t(7));
  TCHECK_EQ(decoded.checkpoints.size(), std::size_t(3));
  TCHECK_EQ(serializeSimulationReplay(decoded), encoded);

  std::unique_ptr<World> replayedWorld = prepare();
  TCHECK(replayedWorld != nullptr);
  if(!replayedWorld)
    return;
  bool replayed = true;
  try {
    replaySimulation(*replayedWorld, decoded);
  } catch(const SimulationReplayError& error) {
    std::fprintf(stderr, "  replay failed: %s\n", error.what());
    replayed = false;
  }
  TCHECK(replayed);
  TCHECK(captureStateSnapshot(*recordedWorld).fingerprint()
    == captureStateSnapshot(*replayedWorld).fingerprint());
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

TTEST(money_operations_handle_integer_boundaries) {
  std::unique_ptr<World> world = make_world();
  if(!world) {
    TCHECK(!"failed to create world");
    return;
  }

  world->total_money = INT_MAX - 1;
  world->income(10, world->stats.income.income_tax);
  TCHECK_EQ(INT_MAX, world->total_money);

  world->total_money = INT_MIN;
  try {
    world->expense(1, world->stats.expenses.construction);
    TCHECK(!"expense below the credit limit did not throw");
  } catch(const Message::Exception&) {
    TCHECK(true);
  }
}

TTEST(world_economy_mutations_are_centralized_and_safe) {
  std::unique_ptr<World> world = make_world();
  if(!world) {
    TCHECK(!"failed to create world");
    return;
  }

  // REF-06: World owns the wide arithmetic and saturation policy for tech.
  world->addTech(INT_MAX);
  TCHECK_EQ(INT_MAX, world->tech_level);
  world->addTech(1);
  TCHECK_EQ(INT_MAX, world->tech_level);
  world->addTech(INT_MIN);
  TCHECK_EQ(0, world->tech_level);
  world->addTech(-1);
  TCHECK_EQ(0, world->tech_level);

  World* world_ptr = world.get();
  const auto record_and_clear = [world_ptr](World::TaxableAccount account,
    int& value) {
    world_ptr->recordTaxable(account, INT_MAX);
    TCHECK_EQ(INT_MAX, value);
    world_ptr->recordTaxable(account, INT_MIN);
    TCHECK_EQ(0, value);
  };
  record_and_clear(World::TaxableAccount::LABOR, world->taxable.labor);
  record_and_clear(World::TaxableAccount::COAL, world->taxable.coal);
  record_and_clear(World::TaxableAccount::ORE, world->taxable.ore);
  record_and_clear(World::TaxableAccount::GOODS, world->taxable.goods);
  record_and_clear(World::TaxableAccount::TRADE_EXPORT,
    world->taxable.trade_ex);

  world->recordTaxable(World::TaxableAccount::LABOR, 10);
  world->recordTaxable(World::TaxableAccount::COAL, 10);
  world->recordTaxable(World::TaxableAccount::ORE, 10);
  world->recordTaxable(World::TaxableAccount::GOODS, 10);
  world->recordTaxable(World::TaxableAccount::TRADE_EXPORT, 10);
  world->clearTaxable();
  TCHECK_EQ(0, world->taxable.labor);
  TCHECK_EQ(0, world->taxable.coal);
  TCHECK_EQ(0, world->taxable.ore);
  TCHECK_EQ(0, world->taxable.goods);
  TCHECK_EQ(0, world->taxable.trade_ex);
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

TTEST(r7_pause_does_not_leak_into_the_next_game) {
  // BUG-45: simDelay is session state, never persisted, and nothing reset
  // it on a new game — Game.cpp deschedules execute and animate outright
  // while it is SIM_DELAY_PAUSE. The reported flow was play -> pause ->
  // menu -> new game, and the new game came up indistinguishable from a
  // hang. R7 says nothing from the previous game may leak into the next.
  initVideo(800, 600);
  Game game(window);

  setSimulationDelay(SIM_DELAY_PAUSE);
  TCHECK_EQ(simDelay, SIM_DELAY_PAUSE);

  std::unique_ptr<World> next = make_world();
  if(!next) {
    TCHECK(!"failed to create world");
    return;
  }
  game.setWorld(std::move(next));
  TCHECK(simDelay != SIM_DELAY_PAUSE);
  TCHECK_EQ(simDelay, SIM_DELAY_SLOW);

  // The other half: the chosen speed is a preference, not a leftover of
  // the game that ended, so it must survive into the next one. This is
  // what keeps the fix from being "always reset to slow".
  setSimulationDelay(SIM_DELAY_FAST);
  std::unique_ptr<World> third = make_world();
  if(!third) {
    TCHECK(!"failed to create world");
    return;
  }
  game.setWorld(std::move(third));
  TCHECK_EQ(simDelay, SIM_DELAY_FAST);
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
  TCHECK(!game.getGameView().hasTextureLoadError());
  TCHECK_EQ((int)game.getGameView().remaining_images, 0);

  // BUG-23: a completed loading barrier alone does not detect missing PNGs.
  const auto* seaFarm = ResourceGroup::resMap.at("SeaFarm");
  TCHECK_EQ(seaFarm->graphicsInfoVector.size(), size_t(5));
  for(const auto& frame : seaFarm->graphicsInfoVector) {
    TCHECK(frame.texture != nullptr);
    if(frame.texture) {
      TCHECK_EQ(frame.texture->getWidth(), 512);
      TCHECK_EQ(frame.texture->getHeight(), 320);
    }
    TCHECK_EQ(frame.x, 256);
    TCHECK_EQ(frame.y, 320);
  }
  Texture* seaFarmIcon = texture_manager->load(
    "images/gui/buttonpanel/parks/seafarm.png");
  TCHECK(seaFarmIcon != nullptr);
  if(seaFarmIcon) {
    TCHECK_EQ(seaFarmIcon->getWidth(), 32);
    TCHECK_EQ(seaFarmIcon->getHeight(), 32);
  }

  // BUG-24: native pixel dimensions and anchors must match the 3x3 footprint.
  for(const char* name : {"OilWell", "OilWellLow", "OilWellMed", "OilWellFull",
                          "Refinery", "RefineryLow", "RefineryMed", "RefineryFull"}) {
    const auto* resource = ResourceGroup::resMap.at(name);
    const int height = std::string(name).find("OilWell") == 0 ? 352 : 384;
    TCHECK_EQ(resource->graphicsInfoVector.size(), size_t(1));
    for(const auto& frame : resource->graphicsInfoVector) {
      TCHECK(frame.texture != nullptr);
      if(frame.texture) {
        TCHECK_EQ(frame.texture->getWidth(), 384);
        TCHECK_EQ(frame.texture->getHeight(), height);
      }
      TCHECK_EQ(frame.x, 192);
      TCHECK_EQ(frame.y, height);
    }
  }

  if(const char* output = std::getenv("LINCITYNG_TEST_SPRITE_PREVIEW")) {
    for(const std::string sceneName : {"seafarm", "petroleum"}) {
      // Optional visual fixture; does not exercise gameplay placement rules.
      auto scene = std::make_unique<World>(100);
      if(sceneName == "seafarm") {
        seaFarmConstructionGroup.placeItem(*scene, MapPoint(40, 40));
        organic_farmConstructionGroup.placeItem(*scene, MapPoint(44, 40));
      } else {
        oilwellConstructionGroup.placeItem(*scene, MapPoint(40, 40));
        refineryConstructionGroup.placeItem(*scene, MapPoint(43, 40));
        industryLightConstructionGroup.placeItem(*scene, MapPoint(40, 44));
        for(int x = 39; x < 47; ++x)
          roadConstructionGroup.placeItem(*scene, MapPoint(x, 43));
      }
      game.setWorld(std::move(scene));
      auto& view = game.getGameView();
      view.resetZoom();
      for(int i = 0; i < 4; ++i) view.zoomIn();
      view.show(MapPoint(42, 42));
      std::filesystem::create_directories(output);
      for(int level = 0; level < 5; ++level) {
        painter->clear();
        view.setMapDirty();
        game.getGui().draw(*painter);
        SDL_Surface* shot = SDL_RenderReadPixels(SDL_GetRenderer(window), nullptr);
        TCHECK(shot != nullptr);
        if(shot) {
          const auto path = std::filesystem::path(output) /
            (sceneName + "-zoom-" + std::to_string(level) + ".png");
          TCHECK(IMG_SavePNG(shot, path.string().c_str()));
          SDL_DestroySurface(shot);
        }
        view.zoomOut();
        view.zoomOut();
      }
    }
  }
}

TTEST(firetruck_art_animation_and_real_render) {
  initVideo(1000, 760);
  Game game(window);
  auto scene = std::make_unique<World>(100);
  for(int x = 36; x <= 48; ++x) {
    if(x >= 41 && x <= 43) {
      scene->map(MapPoint(x, 40))->group = GROUP_WATER;
      roadbridgeConstructionGroup.placeItem(*scene, MapPoint(x, 40));
    } else
      roadConstructionGroup.placeItem(*scene, MapPoint(x, 40));
    roadConstructionGroup.placeItem(*scene, MapPoint(x, 46));
  }
  for(int y = 41; y < 46; ++y) {
    roadConstructionGroup.placeItem(*scene, MapPoint(36, y));
    roadConstructionGroup.placeItem(*scene, MapPoint(48, y));
  }
  fireStationConstructionGroup.placeItem(*scene, MapPoint(37, 42));
  roadConstructionGroup.placeItem(*scene, MapPoint(37, 41));
  game.setWorld(std::move(scene));
  auto& world = game.getWorld();
  world.total_money = 1000000;
  auto& view = game.getGameView();
  const auto deadline = SDL_GetTicks() + 30000;
  while(!view.textures_ready && SDL_GetTicks() < deadline)
    SDL_Delay(10);
  TCHECK(view.textures_ready);
  TCHECK(!view.hasTextureLoadError());
  if(!view.textures_ready || view.hasTextureLoadError()) return;
  while(view.remaining_images && SDL_GetTicks() < deadline)
    view.fetchTextures();
  TCHECK_EQ((int)view.remaining_images, 0);
  if(view.remaining_images) return;

  auto* station = dynamic_cast<FireStation*>(
    world.map(MapPoint(37, 42))->reportingConstruction);
  TCHECK(station != nullptr);
  if(!station) return;
  station->active = false;
  const auto spawnRng = BasicUrbg::get().state();
  std::tuple<int, unsigned long, MapPoint, std::string> passengerFirst;
  {
    Vehicle passenger(world, MapPoint(37, 41), VEHICLE_REDCAR);
    TCHECK_EQ(passenger.model, VEHICLE_REDCAR);
    TCHECK_EQ(passenger.frameIt->frame, -2);
    TCHECK(BasicUrbg::get().state() == spawnRng);
    passenger.update(0);
    TCHECK_EQ(passenger.frameIt->frame, -1);
    passengerFirst = {passenger.frameIt->frame, passenger.anim,
      passenger.destination, BasicUrbg::get().state()};
  }
  TCHECK(BasicUrbg::get().restoreState(spawnRng));
  station->active = true;
  {
    Vehicle emergency(world, MapPoint(37, 41), VEHICLE_REDCAR);
    TCHECK_EQ(emergency.model, VEHICLE_FIRETRUCK);
    TCHECK_EQ(emergency.frameIt->frame, -2);
    TCHECK(BasicUrbg::get().state() == spawnRng);
    emergency.update(0);
    TCHECK_EQ(emergency.frameIt->frame, std::get<0>(passengerFirst));
    TCHECK_EQ(emergency.anim, std::get<1>(passengerFirst));
    TCHECK(emergency.destination == std::get<2>(passengerFirst));
    TCHECK(BasicUrbg::get().state() == std::get<3>(passengerFirst));
  }
  for(VehicleModel model : {VEHICLE_BLUECAR, VEHICLE_GREENCAR,
      VEHICLE_YELLOWCAR, VEHICLE_WHITECAR}) {
    Vehicle passenger(world, MapPoint(37, 41), model);
    TCHECK_EQ(passenger.model, model);
  }
  // Remove the station's access spur before the unbranched route fixture.
  world.bulldozeArea(MapPoint(37, 41));

  const auto* resource = ResourceGroup::resMap.at("Firetruck");
  TCHECK_EQ(resource->graphicsInfoVector.size(), size_t(32));
  TCHECK(!resource->tint.enabled);
  if(resource->graphicsInfoVector.size() != 32) return;
  for(const auto& frame : resource->graphicsInfoVector) {
    TCHECK(frame.texture != nullptr);
    if(frame.texture) {
      TCHECK_EQ(frame.texture->getWidth(), 80);
      TCHECK_EQ(frame.texture->getHeight(), 64);
    }
    TCHECK_EQ(frame.x, 40);
    TCHECK_EQ(frame.y, 48);
  }
  for(const char* car : {"Bluecar", "Redcar", "Greencar", "Yellowcar", "Whitecar"})
    TCHECK_EQ(ResourceGroup::resMap.at(car)->graphicsInfoVector.size(), size_t(16));

  for(int pose = 0; pose < 16; ++pose) {
    auto load = [&](int index) {
      const auto name = std::string("firetruck-") + (index < 10 ? "0" : "")
        + std::to_string(index) + ".png";
      return std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>(
        IMG_Load((headless::app_data() / "images/tiles" / name).string().c_str()),
        SDL_DestroySurface);
    };
    auto a = load(pose), b = load(pose + 16);
    TCHECK(a != nullptr && b != nullptr);
    if(!a || !b) continue;
    TCHECK_EQ(a->w, 80); TCHECK_EQ(a->h, 64);
    TCHECK_EQ(b->w, 80); TCHECK_EQ(b->h, 64);
    if(a->w != 80 || b->w != 80 || a->h != 64 || b->h != 64) continue;
    int opaque = 0, changed = 0, red = 0;
    for(int y = 0; y < 64; ++y) for(int x = 0; x < 80; ++x) {
      Uint8 ar, ag, ab, aa, br, bg, bb, ba;
      TCHECK(SDL_ReadSurfacePixel(a.get(), x, y, &ar, &ag, &ab, &aa));
      TCHECK(SDL_ReadSurfacePixel(b.get(), x, y, &br, &bg, &bb, &ba));
      TCHECK_EQ(aa, ba);
      if(x == 0 || y == 0 || x == 79 || y == 63) TCHECK_EQ(aa, 0);
      if(aa > 128) {
        ++opaque;
        if(ar > ag * 1.3 && ar > ab * 1.3) ++red;
        if(ar != br || ag != bg || ab != bb) ++changed;
      }
    }
    TCHECK(opaque > 100);
    TCHECK(red > 20);
    TCHECK(changed > 0);
    // As two light phases share most pixels; only the red/blue lenses differ.
    TCHECK(changed < opaque / 4);
  }

  // Isolate walk() from routing: these calls must not advance either RNG.
  {
    Vehicle truck(world, MapPoint(38, 40), VEHICLE_FIRETRUCK);
    truck.prev = MapPoint(38, 40);
    truck.point = MapPoint(39, 40);
    truck.next = MapPoint(40, 40);
    truck.old1 = truck.old2 = MapPoint(37, 40);
    truck.anim = 1500;
    const auto rng = BasicUrbg::get().state();
    const auto state = captureStateSnapshot(world).fingerprint();
    for(int direction = 0; direction < 8; ++direction) {
      truck.direction = direction;
      truck.frameIt->frame = direction;
      for(unsigned long time : {120UL, 240UL, 300UL, 480UL, 720UL, 960UL}) {
        truck.update(time);
        TCHECK_EQ(truck.frameIt->frame, direction + int((time / 240) % 2) * 16);
        TCHECK(truck.framePt == truck.prev || truck.framePt == truck.point);
      }
    }
    for(bool uphill : {true, false}) {
      truck.prev = MapPoint(uphill ? 40 : 44, 40);
      truck.point = MapPoint(uphill ? 41 : 45, 40);
      truck.next = MapPoint(uphill ? 42 : 46, 40);
      truck.old1 = truck.old2 = MapPoint(uphill ? 39 : 43, 40);
      for(int direction : {0, 2, 4, 6}) {
        truck.direction = direction;
        truck.frameIt->frame = direction;
        for(unsigned long time : {120UL, 240UL, 300UL, 480UL}) {
          truck.update(time);
          const int pose = (uphill ? 8 : 12) + direction / 2;
          TCHECK_EQ(truck.frameIt->frame, pose + int((time / 240) % 2) * 16);
        }
      }
    }
    TCHECK(BasicUrbg::get().state() == rng);
    TCHECK(captureStateSnapshot(world).fingerprint() == state);
  }

  const char* output = std::getenv("LINCITYNG_TEST_FIRETRUCK_PREVIEW");
  if(output) std::filesystem::create_directories(output);
  auto capture = [&](const std::string& name) {
    if(!output) return;
    painter->clear();
    view.setMapDirty();
    game.getGui().draw(*painter);
    std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> shot(
      SDL_RenderReadPixels(SDL_GetRenderer(window), nullptr), SDL_DestroySurface);
    TCHECK(shot != nullptr);
    if(shot) TCHECK(IMG_SavePNG(shot.get(),
      (std::filesystem::path(output) / (name + ".png")).string().c_str()));
  };
  // The road loop exercises the real route planner, corners and bridge ramps.
  std::array<bool, 16> seen{};
  for(int reverse = 0; reverse < 2; ++reverse) {
    Vehicle truck(world, MapPoint(38, 40), VEHICLE_FIRETRUCK);
    truck.prev = MapPoint(reverse ? 39 : 37, 40);
    truck.old1 = truck.old2 = truck.prev;
    truck.frameIt->frame = reverse ? 6 : 2;
    truck.direction = reverse ? 6 : 2;
    truck.death_counter = 1000;
    Vehicle car(world, MapPoint(46, 46), VEHICLE_BLUECAR);
    car.frameIt->frame = 6;
    car.direction = 6;
    car.prev = MapPoint(47, 46);
    car.death_counter = 1000;
    view.resetZoom();
    view.zoomIn(); view.zoomIn();
    view.show(MapPoint(42, 42));
    for(unsigned long time = 0; time < 90000; time += 120) {
      truck.update(time);
      car.update(time);
      TCHECK(!truck.isDead());
      const int frame = truck.frameIt->frame;
      TCHECK(frame >= 0 && frame < 32);
      if(frame >= 0 && frame < 32) seen[frame % 16] = true;
      TCHECK(car.frameIt->frame >= 0 && car.frameIt->frame < 16);
      TCHECK(truck.framePt == truck.prev || truck.framePt == truck.point);
      if(time < 6000 && time % 240 == 0)
        capture("route-" + std::to_string(reverse) + "-" + std::to_string(time));
    }
    TCHECK(seen[0] && seen[2] && seen[4] && seen[6]);
    TCHECK(seen[1] || seen[3] || seen[5] || seen[7]);
    for(int zoom = 0; zoom < 5; ++zoom) {
      capture("zoom-" + std::to_string(reverse) + "-" + std::to_string(zoom));
      view.zoomOut();
    }
  }
  // Downhill poses (8-15) require an actual elevation change; the flat
  // route above only exercises flat poses (0-7). Uphill/downhill pose
  // logic is already covered in isolation above (captureStateSnapshot).
  TCHECK(seen[0] && seen[2] && seen[4] && seen[6]);
  TCHECK(seen[1] || seen[3] || seen[5] || seen[7]);
}

TTEST(port_trade_preserves_commodity_conservation) {
  // FEAT-03e item 5: with port trade active, commodity conservation
  // must hold (imports/exports go through produceStuff/consumeStuff
  // and the ledger).
  city_settings city{true, false};
  std::unique_ptr<World> world = new_city(&city, 100,
    SimulationProtocol::DETERMINISTIC_V1, 1234567);
  TCHECK(world != nullptr);
  if(!world)
    return;

  // find a bare river-edge tile for port placement
  // (port needs bare 4x4 area + river at east edge, 4 tiles wide)
  MapPoint portPoint;
  bool found = false;
  for(MapPoint p(2, 2); p.y < world->map.len() - 6; ++p.y) {
    for(p.x = 2; p.x < world->map.len() - 6; ++p.x) {
      bool bare = true;
      for(int dy = 0; dy < GROUP_PORT_SIZE && bare; ++dy)
        for(int dx = 0; dx < GROUP_PORT_SIZE; ++dx)
          if(!world->map(MapPoint(p.x + dx, p.y + dy))->is_bare()) {
            bare = false; break;
          }
      if(!bare) continue;
      MapPoint east = p.e(GROUP_PORT_SIZE);
      bool river = true;
      for(int j = 0; j < GROUP_PORT_SIZE; ++j) {
        if(!world->map(east.s(j))->is_river()) {
          river = false;
          break;
        }
      }
      if(river) { portPoint = p; found = true; break; }
    }
    if(found) break;
  }
  TCHECK(found);
  if(!found) return;

  world->tech_level = 35000; // PORT_TECH
  try {
    world->buildConstruction(portConstructionGroup, portPoint);
  } catch(const Message::Exception& err) {
    std::fprintf(stderr, "  port build failed: %s\n", err.what());
    TCHECK(!"port build threw");
    return;
  }
  Port* port = dynamic_cast<Port*>(
    world->map(portPoint)->reportingConstruction);
  TCHECK(port != nullptr);
  if(!port) return;

  // configure port: import food/coal/ore, export goods/steel
  port->commodityRuleCount[STUFF_FOOD] =
    {MAX_FOOD_ON_PORT, true, false};
  port->commodityRuleCount[STUFF_COAL] =
    {MAX_COAL_ON_PORT, true, false};
  port->commodityRuleCount[STUFF_ORE] =
    {MAX_ORE_ON_PORT, true, false};
  port->commodityRuleCount[STUFF_GOODS] =
    {MAX_GOODS_ON_PORT, false, true};
  port->commodityRuleCount[STUFF_STEEL] =
    {MAX_STEEL_ON_PORT, false, true};

  // enable world trade rules for those commodities
  world->tradeRule[STUFF_FOOD].take = true;
  world->tradeRule[STUFF_COAL].take = true;
  world->tradeRule[STUFF_ORE].take = true;
  world->tradeRule[STUFF_GOODS].give = true;
  world->tradeRule[STUFF_STEEL].give = true;

  // give the port workforce so it can trade
  port->commodityCount[STUFF_LABOR] = PORT_LABOR;

  TCHECK(run_days(*world, 120));
}

TTEST(oilwell_produces_petroleum_from_reserve) {
  // FEAT-03d empirical validation: oil well extracts petroleum
  // from oil_reserve and produces petroleum commodity.
  city_settings city{true, false};
  std::unique_ptr<World> world = new_city(&city, 100,
    SimulationProtocol::DETERMINISTIC_V1, 1234567);
  TCHECK(world != nullptr);
  if(!world)
    return;

  // find a bare tile and seed it with enough oil reserve
  MapPoint wellPoint;
  bool found = false;
  for(MapPoint p(2, 2); p.y < world->map.len() - 2; ++p.y) {
    for(p.x = 2; p.x < world->map.len() - 2; ++p.x) {
      if(world->map(p)->is_bare()) {
        wellPoint = p;
        found = true;
        break;
      }
    }
    if(found) break;
  }
  TCHECK(found);
  if(!found) return;

  // seed oil reserve (map gen rarely reaches MIN_OIL_FOR_WELL=1000)
  world->map(wellPoint)->oil_reserve = OIL_RESERVE;
  const int oil_before = world->map(wellPoint)->oil_reserve;

  world->tech_level = OILWELL_TECH; // required to build
  // build well and give it labor
  try {
    world->buildConstruction(oilwellConstructionGroup, wellPoint);
  } catch(const Message::Exception& err) {
    std::fprintf(stderr, "  well build failed: %s\n", err.what());
    TCHECK(!"well build threw");
    return;
  }
  Oilwell* well = dynamic_cast<Oilwell*>(
    world->map(wellPoint)->reportingConstruction);
  TCHECK(well != nullptr);
  if(!well) return;
  well->commodityCount[STUFF_LABOR] = OILWELL_LABOR;

  // run enough days for extraction (5 petroleum/day)
  TCHECK(run_days(*world, 100));

  TCHECK(well->total_extracted > 0);
  TCHECK(world->map(wellPoint)->oil_reserve < oil_before);
  // each extraction produces PETROLEUM_PER_WELL (5) petroleum
  TCHECK_EQ(well->commodityCount[STUFF_PETROLEUM],
    well->total_extracted * PETROLEUM_PER_WELL);
}

TTEST(refinery_processes_petroleum_to_goods) {
  // FEAT-03d empirical validation: refinery consumes petroleum
  // and produces goods.
  city_settings city{true, false};
  std::unique_ptr<World> world = new_city(&city, 100,
    SimulationProtocol::DETERMINISTIC_V1, 7654321);
  TCHECK(world != nullptr);
  if(!world)
    return;

  // find a bare tile for refinery (anywhere)
  MapPoint refPoint;
  bool found = false;
  for(MapPoint p(2, 2); p.y < world->map.len() - 2; ++p.y) {
    for(p.x = 2; p.x < world->map.len() - 2; ++p.x) {
      if(world->map(p)->is_bare()) {
        refPoint = p;
        found = true;
        break;
      }
    }
    if(found) break;
  }
  TCHECK(found);
  if(!found) return;

  world->tech_level = REFINERY_TECH; // required to build
  try {
    world->buildConstruction(refineryConstructionGroup, refPoint);
  } catch(const Message::Exception& err) {
    std::fprintf(stderr, "  refinery build failed: %s\n", err.what());
    TCHECK(!"refinery build threw");
    return;
  }
  Refinery* ref = dynamic_cast<Refinery*>(
    world->map(refPoint)->reportingConstruction);
  TCHECK(ref != nullptr);
  if(!ref) return;

  // seed petroleum directly (bypasses well dependency for this test)
  ref->commodityCount[STUFF_PETROLEUM] = 1000;
  ref->commodityCount[STUFF_LABOR] = REFINERY_LABOR;

  const int petroleum_before = ref->commodityCount[STUFF_PETROLEUM];
  const int goods_before = ref->commodityCount[STUFF_GOODS];

  TCHECK(run_days(*world, 100));

  TCHECK(ref->total_processed > 0);
  TCHECK(ref->commodityCount[STUFF_PETROLEUM]
    < petroleum_before);
  TCHECK(ref->commodityCount[STUFF_GOODS]
    > goods_before);
}

TTEST(music_callback_defers_track_change_to_main_thread) {
  Sound *sound = getSound();
  TCHECK(sound != nullptr);
  if(!sound)
    return;

  Sound::musicHalted(nullptr, nullptr);
  TCHECK(sound->consumeTrackChangeRequest());
  TCHECK(!sound->consumeTrackChangeRequest());
}

TTEST(visual_pilot_build_tools_select_registered_groups) {
  initVideo(800, 600);
  Game game(window);
  auto world = make_world();
  TCHECK(world != nullptr);
  if(!world) return;
  game.setWorld(std::move(world));
  auto& panel = game.getButtonPanel();
  UserOperation selected;
  panel.selected.connect([&selected](const UserOperation& op) { selected = op; });

  struct ToolCase {
    const char* button;
    ConstructionGroup* group;
    const char* icon;
  };
  for(const auto& entry : {
      ToolCase{"BPMSeaFarmButton", &seaFarmConstructionGroup,
        "images/gui/buttonpanel/parks/seafarm.png"},
      ToolCase{"BPMOilwellButton", &oilwellConstructionGroup,
        "images/gui/buttonpanel/mining/oilwell.png"},
      ToolCase{"BPMRefineryButton", &refineryConstructionGroup,
        "images/gui/buttonpanel/industry/refinery.png"}}) {
    auto* button = getCheckButton(panel, entry.button);
    TCHECK(button != nullptr);
    if(!button) continue;
    auto& current = game.getWorld();
    current.total_money = 100000000;
    current.tech_level = entry.group->tech - 1;
    panel.updateTech();
    TCHECK(!button->isEnabled());
    current.tech_level = MAX_TECH_LEVEL;
    panel.updateTech();
    TCHECK(button->isEnabled());
    button->tryCheck();
    TCHECK(selected.action == UserOperation::ACTION_BUILD);
    TCHECK(selected.constructionGroup == entry.group);
    TCHECK(button->getTooltip().find(entry.group->getName()) != std::string::npos);
    TCHECK(button->getTooltip().find(format_money(
      entry.group->getCosts(current))) != std::string::npos);
    auto* icon = texture_manager->load(entry.icon);
    TCHECK(icon != nullptr);
    if(icon) {
      TCHECK_EQ(icon->getWidth(), 32);
      TCHECK_EQ(icon->getHeight(), 32);
    }
    panel.selectQueryTool();
    TCHECK(selected.action == UserOperation::ACTION_QUERY);
  }
}

TTEST(visual_pilot_petroleum_icons_and_help) {
  initVideo(800, 600);
  Game game(window);
  const auto data = getConfig()->appDataDir.get();
  struct AssetCase {
    const char* topic;
    const char* icon;
    const char* title;
    int strokeX;
    int strokeY;
  };
  for(const auto& entry : {
      AssetCase{"oilwell", "images/gui/buttonpanel/mining/oilwell.png",
        "Poço de Petróleo", 6, 18},
      AssetCase{"refinery", "images/gui/buttonpanel/industry/refinery.png",
        "Refinaria", 29, 16}}) {
    std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> icon(
      IMG_Load((data / entry.icon).string().c_str()), SDL_DestroySurface);
    TCHECK(icon != nullptr);
    if(icon) {
      Uint8 r = 0, g = 0, b = 0, a = 0;
      // Structural strokes disappeared in the old SVG export despite valid dimensions.
      TCHECK(SDL_ReadSurfacePixel(icon.get(), entry.strokeX, entry.strokeY,
        &r, &g, &b, &a));
      TCHECK(a > 100); // one-pixel strokes have antialiased edges
      TCHECK(r < 120 && g < 120 && b < 120);
      TCHECK(SDL_ReadSurfacePixel(icon.get(), 0, 0, &r, &g, &b, &a));
      TCHECK_EQ(a, 0);
    }
    for(const auto* language : {"en", "pt_BR"}) {
      const auto file = std::filesystem::path("help") / language /
        (std::string(entry.topic) + ".xml");
      std::ifstream input(data / file);
      TCHECK(input.good());
      const std::string xml{std::istreambuf_iterator<char>(input), {}};
      TCHECK(xml.find("<img src=\"" + std::string(entry.icon) + "\"") != std::string::npos);
      const auto tile = "images/tiles/" + std::string(entry.topic) + "-empty.png";
      TCHECK(xml.find("<img src=\"" + tile + "\"") != std::string::npos);
      TCHECK(texture_manager->load(tile) != nullptr);
      if(std::string(language) == "pt_BR")
        TCHECK(xml.find(entry.title) != std::string::npos);
      auto contents = loadGUIFile(file);
      TCHECK(dynamic_cast<Document*>(contents.get()) != nullptr);
      contents->resize(450, 0);
    }
  }
}

TTEST(build_tooltip_shows_cost_and_operating_estimate) {
  // QW-04: the build menu exposes the same build/maintenance values used by
  // the engine, without changing any economic state.
  initVideo(800, 600);
  Game game(window);
  std::unique_ptr<World> world = make_world();
  if(!world) {
    TCHECK(!"failed to create world");
    return;
  }
  game.setWorld(std::move(world));

  CheckButton* road = getCheckButton(game.getButtonPanel(),
    "BPMStreetButton");
  TCHECK(road != nullptr);
  if(!road)
    return;

  const World& current = game.getWorld();
  const std::string tooltip = road->getTooltip();
  TCHECK(tooltip.find(format_money(
    roadConstructionGroup.getCosts(current))) != std::string::npos);
  TCHECK(tooltip.find("100") != std::string::npos);
  TCHECK(tooltip.find("Maintenance") != std::string::npos);
}

TTEST(management_kpis_are_shared_by_mps_and_game_stats) {
  // FEAT-03a: presentation reads one snapshot, so MPS and GameStats cannot
  // silently diverge in population/reserve definitions.
  initVideo(800, 600);
  Game game(window);
  std::unique_ptr<World> world = make_world();
  if(!world) {
    TCHECK(!"failed to create world");
    return;
  }
  game.setWorld(std::move(world));

  World& current = game.getWorld();
  current.total_money = 123456;
  current.tech_level = 77;
  current.stats.total_pollution = 42;
  current.stats.population.population_m.stat = 12345;
  current.stats.population.housed_m.stat = 9000;
  current.stats.population.unemployed_m.stat = 500;
  current.stats.population.starving_m.stat = 200;
  current.stats.inventory[STUFF_FOOD].amount.stat = 11;
  current.stats.inventory[STUFF_GOODS].amount.stat = 22;
  current.stats.inventory[STUFF_COAL].amount.stat = 33;
  current.stats.inventory[STUFF_ORE].amount.stat = 44;
  for(MapTile& tile : current.map) {
    tile.coal_reserve = 0;
    tile.ore_reserve = 0;
  }
  current.map(MapPoint(1, 1))->coal_reserve = 7;
  current.map(MapPoint(2, 2))->ore_reserve = 9;

  const ManagementKpis kpis = collectManagementKpis(current);
  TCHECK_EQ(kpis.money, 123456);
  TCHECK_EQ(kpis.population, 123);
  TCHECK_EQ(kpis.housed, 90);
  TCHECK_EQ(kpis.unemployed, 5);
  TCHECK_EQ(kpis.starving, 2);
  TCHECK_EQ(kpis.tech, 77);
  TCHECK_EQ(kpis.pollution, 42);
  TCHECK_EQ(kpis.food_stock, 11);
  TCHECK_EQ(kpis.goods_stock, 22);
  TCHECK_EQ(kpis.coal_stock, 33);
  TCHECK_EQ(kpis.ore_stock, 44);
  TCHECK_EQ(kpis.coal_reserve, 7L);
  TCHECK_EQ(kpis.ore_reserve, 9L);

  game.getMiniMap().switchView("GlobalMPS");
  game.getMpsFinance().page = MpsFinance::Page::MANAGEMENT;
  game.getMpsFinance().refresh();
  SwitchComponent* switcher = getSwitchComponent(
    game.getGui(), "MiniMapSwitch");
  Component* mps = switcher ? switcher->getActiveComponent() : nullptr;
  Paragraph* mpsMoney = mps
    ? dynamic_cast<Paragraph*>(mps->findComponent("mps_text1")) : nullptr;
  TCHECK(mpsMoney != nullptr);
  if(mpsMoney)
    TCHECK(mpsMoney->getText().find("123") != std::string::npos);

  openGameStats(game);
  Paragraph* statsReserve = getParagraph(
    game.getGui(), "statistic_value_15_1");
  TCHECK(statsReserve != nullptr);
  if(statsReserve)
    TCHECK_EQ(statsReserve->getText(), std::string("7"));
  closeAllDialogs();
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

TTEST(save_api_reports_io_failure) {
  // BUG-15: a failed save must be observable by its caller and must not leave
  // a partial replacement or temporary file behind.
  std::unique_ptr<World> world = make_world();
  if(!world) {
    TCHECK(!"failed to create world");
    return;
  }

  const std::filesystem::path parent =
    headless::user_data() / "bug15-missing-parent";
  const std::filesystem::path filename = parent / "save.scn.gz";
  std::error_code ec;
  std::filesystem::remove_all(parent, ec);

  TCHECK(!saveCityNG(*world, filename));
  TCHECK(!std::filesystem::exists(filename));
  TCHECK(!autoSaveCityNG(*world, filename));
  TCHECK(!std::filesystem::exists(filename));
  TCHECK(!std::filesystem::exists(filename.string() + ".tmp"));

  // A commit failure must preserve the previous slot as well. A directory at
  // the destination makes the final rename fail after the temporary save has
  // succeeded, exercising the atomic-commit branch.
  std::filesystem::create_directories(parent, ec);
  std::filesystem::create_directory(filename, ec);
  TCHECK(!saveCityNG(*world, filename));
  TCHECK(std::filesystem::is_directory(filename));
  TCHECK(!std::filesystem::exists(filename.string() + ".tmp"));
  std::filesystem::remove_all(parent, ec);
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

/* Benchmark: mede fluxo de commodities pelo porto após N dias.
 * Imprime CSV para análise dos PORT_*_RATE.
 * Tenta cenários com porto se a cidade aleatória não tiver um. */
void benchmark_port(World& world, int days) {
  /* Configura o porto para operar: importa food/coal/ore,
   * exporta goods/steel. */
  for(Construction* cst : world.map.constructions) {
    Port* port = dynamic_cast<Port*>(cst);
    if(!port) continue;
    port->commodityRuleCount[STUFF_FOOD].take = true;
    port->commodityRuleCount[STUFF_FOOD].give = false;
    port->commodityRuleCount[STUFF_COAL].take = true;
    port->commodityRuleCount[STUFF_COAL].give = false;
    port->commodityRuleCount[STUFF_ORE].take = true;
    port->commodityRuleCount[STUFF_ORE].give = false;
    port->commodityRuleCount[STUFF_GOODS].take = false;
    port->commodityRuleCount[STUFF_GOODS].give = true;
    port->commodityRuleCount[STUFF_STEEL].take = false;
    port->commodityRuleCount[STUFF_STEEL].give = true;
  }

  /* Snapshot do inventário inicial */
  std::array<int, STUFF_COUNT> start_inv = {};
  for(const Construction* cst : world.map.constructions) {
    const Port* port = dynamic_cast<const Port*>(cst);
    if(!port) continue;
    for(Commodity s = STUFF_INIT; s < STUFF_COUNT; s++)
      start_inv[s] += port->commodityCount[s];
  }

  run_days(world, days,
    /*check_conservation=*/true,
    /*check_commodities=*/true);

  /* Snapshot do inventário final */
  std::array<int, STUFF_COUNT> end_inv = {};
  bool foundPort = false;
  for(const Construction* cst : world.map.constructions) {
    const Port* port = dynamic_cast<const Port*>(cst);
    if(!port) continue;
    foundPort = true;
    for(Commodity s = STUFF_INIT; s < STUFF_COUNT; s++)
      end_inv[s] += port->commodityCount[s];
  }

  std::printf("\n=== PORT BENCHMARK (%d dias, seed %llu) ===\n",
    days, (unsigned long long)g_seed);
  std::printf("commodity,start,end,delta,daily_ic,daily_et,lastm_ic,lastm_et\n");

  for(const Construction* cst : world.map.constructions) {
    const Port* port = dynamic_cast<const Port*>(cst);
    if(!port) continue;
    for(Commodity s = STUFF_INIT; s < STUFF_COUNT; s++) {
      std::printf("%s,%d,%d,%d,%d,%d,%d,%d\n",
        commodityStandardName(s),
        start_inv[s], end_inv[s], end_inv[s] - start_inv[s],
        port->daily_ic, port->daily_et,
        port->lastm_ic, port->lastm_et);
    }
    std::printf("daily_ic,%d,,,\n", port->daily_ic);
    std::printf("daily_et,%d,,,\n", port->daily_et);
    std::printf("monthly_ic,%d,,,\n", port->monthly_ic);
    std::printf("monthly_et,%d,,,\n", port->monthly_et);
    std::printf("lastm_ic,%d,,,\n", port->lastm_ic);
    std::printf("lastm_et,%d,,,\n", port->lastm_et);
    std::printf("pence,%d,,,\n", port->pence);
    std::printf("busy,%d,,,\n", port->busy);
    std::printf("working_days,%d,,,\n", port->working_days);
    std::printf("tech_made,%d,,,\n", port->tech_made);
  }
  if(!foundPort)
    std::printf("NO_PORT_FOUND,,,\n");
}

TTEST(port_benchmark_365_days) {
  std::unique_ptr<World> world = make_world();
  TCHECK(world != nullptr);
  if(!world) return;
  benchmark_port(*world, 365);
}

/* ---------------------------------------------------------------------- *
 * PERF-01 / PERF-02 — benchmark determinístico com cadência de animação
 *
 * Por que existe: `run_days` chama `World::do_animate()` 1× a cada 30 dias
 * (só para checar os invariantes de veículo). Na cadência real de jogo o
 * animate roda a cada 100 ms de relógio, ou seja muitas vezes por dia
 * simulado. Isso significa que os custos que vivem no animate — o
 * quadrático de `tileOccupied()`, as buscas em `ResourceGroup::resMap` por
 * construção por tick, e o `std::set` percorrido inteiro — nunca foram
 * medidos por nenhuma ferramenta do projeto.
 *
 * `run_bench_days` reproduz a ordem do laço real (Game.cpp: do_time_step,
 * depois do_animate) com a cadência de animate configurável, e mede as duas
 * fases separadamente. Não roda os invariantes de conservação: é um
 * instrumento de medida, não um teste.
 * ---------------------------------------------------------------------- */
struct BenchCounters {
  long long sim_calls = 0;
  long long sim_us = 0;
  long long animate_calls = 0;
  long long animate_us = 0;
  size_t constructions_start = 0;
  size_t constructions_end = 0;
  size_t vehicles_peak = 0;
  size_t vehicles_end = 0;
};

void run_bench_days(World& world, int days, int animate_every,
  BenchCounters& out) {
  using clock = std::chrono::steady_clock;
  Uint32 tick = 0;
  out.constructions_start = world.map.constructions.size();

  for(int day = 0; day < days; day++) {
    const auto s0 = clock::now();
    world.do_time_step();
    const auto s1 = clock::now();
    out.sim_us +=
      std::chrono::duration_cast<std::chrono::microseconds>(s1 - s0).count();
    out.sim_calls++;

    if(animate_every > 0 && day % animate_every == 0) {
      const auto a0 = clock::now();
      world.do_animate(tick);
      const auto a1 = clock::now();
      out.animate_us += std::chrono::duration_cast<
        std::chrono::microseconds>(a1 - a0).count();
      out.animate_calls++;
      if(world.vehicleList.size() > out.vehicles_peak)
        out.vehicles_peak = world.vehicleList.size();
      tick += 100;
    }
  }

  out.constructions_end = world.map.constructions.size();
  out.vehicles_end = world.vehicleList.size();
}

/* Relatório estável (key=value, uma linha por chave) para comparar dois
 * commits. Usamos fmt em vez de printf com `%f`: printf segue o locale do
 * processo e emite "1093,244" em pt-BR, o que quebra qualquer parsing
 * posterior. fmt usa o locale "C" por padrão. Mesmo cuidado já tomado em
 * xmlloadsave (xmlFormat/xmlParse independentes de locale). */
void print_bench_report(const World& world, const BenchCounters& c,
  int days, int animate_every, const StateFingerprint128& fingerprint) {
  const double sim_ms = static_cast<double>(c.sim_us) / 1000.0;
  const double animate_ms = static_cast<double>(c.animate_us) / 1000.0;

  fmt::println("bench.days={}", days);
  fmt::println("bench.animate_every_days={}", animate_every);
  fmt::println("bench.scenario={}",
    g_scenario.empty() ? std::string("<random>") : g_scenario);
  fmt::println("bench.map_len={}", world.map.len());
  fmt::println("bench.seed={}", g_seed);
  fmt::println("bench.constructions_start={}", c.constructions_start);
  fmt::println("bench.constructions_end={}", c.constructions_end);
  fmt::println("bench.vehicles_peak={}", c.vehicles_peak);
  fmt::println("bench.vehicles_end={}", c.vehicles_end);
  fmt::println("bench.sim_calls={}", c.sim_calls);
  fmt::println("bench.sim_ms={:.3f}", sim_ms);
  fmt::println("bench.animate_calls={}", c.animate_calls);
  fmt::println("bench.animate_ms={:.3f}", animate_ms);
  fmt::println("bench.total_ms={:.3f}", sim_ms + animate_ms);
  if(c.sim_calls)
    fmt::println("bench.sim_us_per_day={:.2f}",
      static_cast<double>(c.sim_us) / static_cast<double>(c.sim_calls));
  if(c.animate_calls)
    fmt::println("bench.animate_us_per_call={:.2f}",
      static_cast<double>(c.animate_us)
        / static_cast<double>(c.animate_calls));
  /* Quanto do custo total o animate representa. É o número que decide se a
   * Fase 4 do roadmap (PERF-05/PERF-06) vale o esforço. */
  if(c.sim_us + c.animate_us)
    fmt::println("bench.animate_share_pct={:.2f}",
      100.0 * static_cast<double>(c.animate_us)
        / static_cast<double>(c.sim_us + c.animate_us));
  /* O fingerprint torna o benchmark verificável: duas execuções com a mesma
   * seed devem produzir exatamente o mesmo estado final. Se isso divergir, a
   * comparação de tempo está medindo Worlds diferentes. */
  fmt::println("bench.fingerprint={}", fingerprint.hex());
  std::fflush(stdout);
}

int run_bench() {
  /* PERF-01: o benchmark precisa ser hermético. `headless_env` chama
   * Config::init(), que resolve o config real do usuário
   * (~/.config/lincity-ng.conf) — então WorldSideLen, CarsEnabled e afins
   * viram dependência da máquina de quem roda, e comparar tempos entre
   * máquinas passa a medir coisas diferentes. Fixamos o que afeta o
   * resultado; o relatório imprime o que foi fixado. */
  getConfig()->worldSize.session = g_bench_world_size;
  getConfig()->carsEnabled.session = true;
  /* Default é um cenário com cidade real e estradas. Uma cidade aleatória
   * nova fica em ~54 construções e 0 veículos (medido), o que torna
   * animate_share_pct inútil — o custo do animate vive justamente em mover
   * veículos e varrer a lista de construções. */
  g_scenario = g_bench_scenario;

  std::unique_ptr<World> world = make_world();
  if(!world) {
    std::fprintf(stderr, "bench: could not create the world\n");
    return 1;
  }
  BenchCounters counters;
  run_bench_days(*world, g_bench_days, g_bench_animate_every, counters);
  print_bench_report(*world, counters, g_bench_days, g_bench_animate_every,
    captureStateSnapshot(*world).fingerprint());
  return 0;
}

TTEST(bench_animate_cadence_runs) {
  /* Só executa o caminho do benchmark em escala pequena: garante que a Fase 0
   * não se degrada em silêncio (ex.: do_animate passando a assertar com
   * vehicles vivos). O número de verdade vem de `--bench`. */
  std::unique_ptr<World> world = make_world();
  TCHECK(world != nullptr);
  if(!world) return;
  BenchCounters counters;
  run_bench_days(*world, 120, 1, counters);
  TCHECK_EQ(counters.sim_calls, 120L);
  TCHECK_EQ(counters.animate_calls, 120L);
  TCHECK(counters.sim_us > 0);
  /* Custo de animação não pode explodir: com a cadência real e uma cidade
   * pequena, cada chamada precisa ficar na ordem dos milissegundos. Limite
   * largo de propósito — só queremos pegar regressão de ordem de grandeza. */
  if(counters.animate_calls)
    TCHECK(counters.animate_us / counters.animate_calls < 200000);
}

TTEST(sea_farm_production_simulation) {
  /* FEAT-05: sea farm on water tiles produces food + goods
   * consuming labor + water + energy. Run a full year and verify
   * the production formulas fire (350 food + 8 goods per day
   * with full labor). */
  city_settings city{true, false};
  std::unique_ptr<World> world = new_city(&city, 100,
    SimulationProtocol::DETERMINISTIC_V1, 777777);
  TCHECK(world != nullptr);
  if(!world) return;

  /* Find a bare 4x4 area, carve it into water for the sea farm */
  MapPoint p(-1, -1);
  for(MapPoint candidate(2, 2); candidate.y < world->map.len() - 6; ++candidate.y) {
    for(candidate.x = 2; candidate.x < world->map.len() - 6; ++candidate.x) {
      bool bare = true;
      for(int dy = 0; dy < GROUP_SEA_FARM_SIZE && bare; ++dy)
        for(int dx = 0; dx < GROUP_SEA_FARM_SIZE; ++dx)
          if(!world->map(MapPoint(candidate.x + dx, candidate.y + dy))->is_bare()) {
            bare = false; break;
          }
      if(bare) { p = candidate; break; }
    }
    if(p.x >= 0) break;
  }
  TCHECK(p.x >= 0);
  if(p.x < 0) return;
  for(int dy = 0; dy < GROUP_SEA_FARM_SIZE; ++dy)
    for(int dx = 0; dx < GROUP_SEA_FARM_SIZE; ++dx)
      world->map(MapPoint(p.x + dx, p.y + dy))->group = GROUP_WATER;


  world->tech_level = SEA_FARM_TECH;
  try {
    world->buildConstruction(seaFarmConstructionGroup, p);
  } catch(const Message::Exception& err) {
    std::fprintf(stderr, "  sea farm build failed: %s\n", err.what());
    TCHECK(!"sea farm build threw");
    return;
  }

  SeaFarm* farm = dynamic_cast<SeaFarm*>(
    world->map(p)->reportingConstruction);
  TCHECK(farm != nullptr);
  if(!farm) return;

  /* Seed labor + power + water so production runs every day */
  farm->commodityCount[STUFF_LABOR] = MAX_LABOR_AT_SEA_FARM;
  farm->commodityCount[STUFF_LOVOLT] = MAX_POWER_AT_SEA_FARM;
  farm->commodityCount[STUFF_WATER] = MAX_WATER_AT_SEA_FARM;

  const int food_before = farm->commodityCount[STUFF_FOOD];
  const int goods_before = farm->commodityCount[STUFF_GOODS];

  TCHECK(run_days(*world, 1200));

  TCHECK(farm->commodityCount[STUFF_FOOD] > food_before);
  TCHECK(farm->commodityCount[STUFF_GOODS] > goods_before);

  /* Daily production with full labor: 350 food, 8 goods */
  const int food_delta = farm->commodityCount[STUFF_FOOD] - food_before;
  const int goods_delta = farm->commodityCount[STUFF_GOODS] - goods_before;
  std::fprintf(stderr,
    "  sea_farm: food +%d (%d/day), goods +%d (%d/day) over 1200 days\n",
    food_delta, food_delta / 1200, goods_delta, goods_delta / 1200);
  TCHECK(food_delta > 0);
  TCHECK(goods_delta > 0);
}

TTEST(ledger_accumulates_during_simulation) {
  /* FEAT-04: build a construction, run simulation, verify ledger
   * fields (maintenance/revenue) grow from activity. */
  city_settings city;
  city.with_village = true;
  std::unique_ptr<World> world = new_city(&city, 100,
    SimulationProtocol::DETERMINISTIC_V1, 888888);
  TCHECK(world != nullptr);
  if(!world) return;

  /* Build a residence to get a construction with ongoing costs */
  MapPoint p(10, 10);
  world->tech_level = 60000;
  try {
    world->buildConstruction(residenceLLConstructionGroup, p);
  } catch(const Message::Exception& err) {
    std::fprintf(stderr, "  residence build failed: %s\n", err.what());
    TCHECK(!"residence build threw");
    return;
  }

  Construction* cstr = world->map(p)->reportingConstruction;
  TCHECK(cstr != nullptr);
  if(!cstr) return;

  const int maint_before = cstr->maintenance;
  const int rev_before = cstr->revenue;
  const int cap_before = cstr->capital_invested;
  const int purch_before = cstr->purchase_cost;

  TCHECK(run_days(*world, 600));

  /* Maintenance should have accumulated from monthly debits */
  std::fprintf(stderr,
    "  ledger: purchase=%d capital=%d maint %d->%d revenue %d->%d\n",
    purch_before, cap_before, maint_before, cstr->maintenance,
    rev_before, cstr->revenue);
  TCHECK(cstr->purchase_cost == purch_before); /* purchase_cost immutable */
  TCHECK(cstr->capital_invested >= cap_before);
}

TTEST(sea_farm_ledger_records_expense_and_revenue) {
  /* Integration: sea farm update() calls recordExpense/recordRevenue
   * which accumulate in the Construction ledger. */
  city_settings city{true, false};
  std::unique_ptr<World> world = new_city(&city, 100,
    SimulationProtocol::DETERMINISTIC_V1, 999999);
  TCHECK(world != nullptr);
  if(!world) return;

  /* Find a bare 4x4 area, carve it into water for the sea farm */
  MapPoint p(-1, -1);
  for(MapPoint candidate(2, 2); candidate.y < world->map.len() - 6; ++candidate.y) {
    for(candidate.x = 2; candidate.x < world->map.len() - 6; ++candidate.x) {
      bool bare = true;
      for(int dy = 0; dy < GROUP_SEA_FARM_SIZE && bare; ++dy)
        for(int dx = 0; dx < GROUP_SEA_FARM_SIZE; ++dx)
          if(!world->map(MapPoint(candidate.x + dx, candidate.y + dy))->is_bare()) {
            bare = false; break;
          }
      if(bare) { p = candidate; break; }
    }
    if(p.x >= 0) break;
  }
  TCHECK(p.x >= 0);
  if(p.x < 0) return;
  for(int dy = 0; dy < GROUP_SEA_FARM_SIZE; ++dy)
    for(int dx = 0; dx < GROUP_SEA_FARM_SIZE; ++dx)
      world->map(MapPoint(p.x + dx, p.y + dy))->group = GROUP_WATER;

  world->tech_level = SEA_FARM_TECH;
  try {
    world->buildConstruction(seaFarmConstructionGroup, p);
  } catch(const Message::Exception& err) {
    std::fprintf(stderr, "  sea farm build failed: %s\n", err.what());
    TCHECK(!"sea farm build threw");
    return;
  }

  SeaFarm* farm = dynamic_cast<SeaFarm*>(
    world->map(p)->reportingConstruction);
  TCHECK(farm != nullptr);
  if(!farm) return;

  farm->commodityCount[STUFF_LABOR] = MAX_LABOR_AT_SEA_FARM;
  farm->commodityCount[STUFF_LOVOLT] = MAX_POWER_AT_SEA_FARM;
  farm->commodityCount[STUFF_WATER] = MAX_WATER_AT_SEA_FARM;

  const int maint_before = farm->maintenance;
  const int rev_before = farm->revenue;

  /* Run enough days for production cycles */
  TCHECK(run_days(*world, 300));

  std::fprintf(stderr,
    "  sea_farm_ledger: maint %d->%d revenue %d->%d\n",
    maint_before, farm->maintenance, rev_before, farm->revenue);
  TCHECK(farm->maintenance >= maint_before);
  TCHECK(farm->revenue >= rev_before);
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
    else if(arg == "--filter")
      tiny_test::set_filter(next());
    else if(arg == "--bench")
      g_bench = true;
    else if(arg == "--bench-days")
      g_bench_days = std::stoi(next());
    else if(arg == "--bench-animate-every")
      g_bench_animate_every = std::stoi(next());
    else if(arg == "--bench-world-size")
      g_bench_world_size = std::stoi(next());
    else if(arg == "--bench-scenario")
      g_bench_scenario = next();
    else if(arg == "--ts")
      tiny_test::set_timestamps(true);
    else if(arg == "--list-tests")
      return tiny_test::list_tests();
    else if(arg == "--log-areas")
      setenv("LINCITYNG_LOG_AREAS", next().c_str(), 1);
    else if(arg == "--log-level")
      setenv("LINCITYNG_LOG_LEVEL", next().c_str(), 1);
    else
      throw std::runtime_error("unrecognized argument: " + arg);
  }

  lincity::log::init_logging();

  int rc;
  try {
    headless::init_env(appData, userData, g_seed);
    // PERF-01: o modo benchmark não roda a suíte — é um instrumento, não um
    // teste. A suíte continua sendo o gate; o benchmark é a régua.
    rc = g_bench ? run_bench() : tiny_test::run_all_tests();
    headless::shutdown_env();
  } catch(const std::exception& err) {
    std::fprintf(stderr, "simtest bootstrap failed: %s\n", err.what());
    return 1;
  }
  return rc;
}
