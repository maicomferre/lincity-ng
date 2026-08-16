/* savetest.cpp - save round-trip tests (test layer 3).
 *
 * For every scenario in data/opening/: load -> save -> load, then compare
 * the key persisted state (money, tech, time, rates, taxable, construction
 * count). Any change to xmlloadsave.cpp or to persisted state (ldsv_version
 * bump, R3) must keep this green.
 */

#include "../tiny_test.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>

#include "headless_env.hpp"

#include "lincity-ng/Config.hpp"   // for getConfig
#include "lincity/init_game.hpp"   // for city_settings, new_city
#include "lincity/world.hpp"       // for World

namespace {

bool compare_worlds(const World& a, const World& b,
    const std::string& label) {
  bool ok = true;
  auto check = [&](const char* what, long long av, long long bv) {
    if(av != bv) {
      std::fprintf(stderr, "  %s: %s differs after round-trip: %lld vs %lld\n",
        label.c_str(), what, av, bv);
      ok = false;
    }
  };
  check("total_money", a.total_money, b.total_money);
  check("tech_level", a.tech_level, b.tech_level);
  check("total_time", a.total_time, b.total_time);
  check("people_pool", a.people_pool, b.people_pool);
  check("given_scene size", a.given_scene.size(), b.given_scene.size());
  check("money_rates.income_tax", a.money_rates.income_tax,
    b.money_rates.income_tax);
  check("money_rates.coal_tax", a.money_rates.coal_tax,
    b.money_rates.coal_tax);
  check("money_rates.ore_tax", a.money_rates.ore_tax,
    b.money_rates.ore_tax);
  check("money_rates.dole", a.money_rates.dole, b.money_rates.dole);
  check("money_rates.transport_cost", a.money_rates.transport_cost,
    b.money_rates.transport_cost);
  check("money_rates.goods_tax", a.money_rates.goods_tax,
    b.money_rates.goods_tax);
  check("money_rates.export_tax", a.money_rates.export_tax,
    b.money_rates.export_tax);
  check("money_rates.import_cost", a.money_rates.import_cost,
    b.money_rates.import_cost);
  check("taxable.labor", a.taxable.labor, b.taxable.labor);
  check("taxable.coal", a.taxable.coal, b.taxable.coal);
  check("taxable.ore", a.taxable.ore, b.taxable.ore);
  check("taxable.goods", a.taxable.goods, b.taxable.goods);
  check("taxable.trade_ex", a.taxable.trade_ex, b.taxable.trade_ex);
  check("constructions", (long long)a.map.constructions.size(),
    (long long)b.map.constructions.size());
  return ok;
}

TTEST(round_trip_all_scenarios) {
  bool ok = true;
  int roundtripped = 0;
  for(const auto& entry :
    std::filesystem::directory_iterator(headless::app_data() / "opening")
  ) {
    if(!entry.is_regular_file())
      continue;
    if(entry.path().extension() != ".gz")
      continue;
    if(entry.path().filename().string().rfind(".scn.gz") == std::string::npos)
      continue;

    const std::string name = entry.path().filename().string();
    const std::filesystem::path tmp =
      headless::user_data() / ("roundtrip_" + name);

    std::unique_ptr<World> world;
    try {
      world = World::load(entry.path());
    } catch(const std::exception& err) {
      std::fprintf(stderr, "  failed to load %s: %s\n", name.c_str(),
        err.what());
      ok = false;
      continue;
    }
    if(!world) {
      std::fprintf(stderr, "  failed to load %s\n", name.c_str());
      ok = false;
      continue;
    }

    try {
      world->save(tmp);
    } catch(const std::exception& err) {
      std::fprintf(stderr, "  failed to save %s: %s\n", name.c_str(),
        err.what());
      ok = false;
      std::filesystem::remove(tmp);
      continue;
    }

    std::unique_ptr<World> reloaded = World::load(tmp);
    if(!reloaded) {
      std::fprintf(stderr, "  failed to reload %s\n", name.c_str());
      ok = false;
    } else {
      ok = compare_worlds(*world, *reloaded, name) && ok;
      roundtripped++;
    }
    std::error_code ec;
    std::filesystem::remove(tmp, ec);
  }
  TCHECK(ok);
  TCHECK(roundtripped >= 6);
}

TTEST(round_trip_new_city) {
  // Round-trip a freshly generated city too, not only the fixed scenarios.
  const std::filesystem::path tmp =
    headless::user_data() / "roundtrip_new_city.scn.gz";
  std::filesystem::remove(tmp);

  city_settings city;
  city.with_village = true;
  city.without_trees = false;
  std::unique_ptr<World> world =
    new_city(&city, getConfig()->worldSize.get());
  TCHECK(world != nullptr);
  if(!world)
    return;

  world->save(tmp);
  std::unique_ptr<World> reloaded = World::load(tmp);
  TCHECK(reloaded != nullptr);
  if(!reloaded)
    return;

  TCHECK(compare_worlds(*world, *reloaded, "new_city"));
  std::error_code ec;
  std::filesystem::remove(tmp, ec);
}

} // namespace

int main(int argc, char** argv) {
  std::filesystem::path appData = LINCITYNG_TEST_SOURCE_DIR "/data";
  std::filesystem::path userData = std::filesystem::temp_directory_path()
    / "lincity-ng-test-save";
  const uint64_t seed = 424242;

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
    else
      throw std::runtime_error("unrecognized argument: " + arg);
  }

  int rc;
  try {
    headless::init_env(appData, userData, seed);
    rc = tiny_test::run_all_tests();
    headless::shutdown_env();
  } catch(const std::exception& err) {
    std::fprintf(stderr, "savetest bootstrap failed: %s\n", err.what());
    return 1;
  }
  return rc;
}
