/* simtest.cpp - headless simulation harness (test layer 2).
 *
 * Boots the engine without a real display (SDL dummy drivers), runs the
 * simulation for a fixed number of days with a fixed RNG seed, and checks
 * invariants:
 *   - money conservation: sum of money accounts matches the cash delta
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

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "headless_env.hpp"

#include "lincity-ng/Config.hpp"             // for getConfig
#include "lincity/init_game.hpp"             // for city_settings, new_city
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
 * check_conservation is disabled for worlds that contain schools or wind
 * power: School::update and Windpower::update accrue their expense
 * accounts without debiting money (BUG-03b), which breaks exactness for
 * them. Enable it again for those worlds when BUG-03b lands. */
bool run_days(World& world, int days, bool check_conservation = true) {
  Uint32 tick = 0;
  // Accounts were finalized at the last in-game year boundary; loaded worlds
  // start with empty accounts, so rebasing to their balance is also exact.
  g_year_base_money = world.total_money;
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
    // Scenarios contain schools and wind power, whose accounts don't match
    // the cash yet (BUG-03b); smoke-test them for now and enable the
    // conservation check when that bug is fixed.
    TCHECK(run_days(*world, 1200, false));
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

} // namespace

int main(int argc, char** argv) {
  std::filesystem::path appData = LINCITYNG_TEST_SOURCE_DIR "/data";
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
