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
#include "lincity/SimulationRandom.hpp" // for SimulationProtocol
#include "lincity/StateFingerprint.hpp" // for captureStateSnapshot
#include "lincity/init_game.hpp"   // for city_settings, new_city
#include "lincity/lintypes.hpp"    // for Construction state comparisons
#include "lincity/modules/port.hpp" // for Port, portConstructionGroup
#include "lincity/world.hpp"       // for World
#include "lincity/xmlloadsave.hpp" // for serializeWorldGameplayState
#include "util/debuglog.hpp"       // for LNG_LOG, init_logging
#include "util/randutil.hpp"        // for BasicUrbg

namespace {

void report_canonical_difference(const std::string& before,
    const std::string& after, const char* label) {
  const std::size_t common = std::min(before.size(), after.size());
  std::size_t index = 0;
  while(index < common && before[index] == after[index])
    index++;
  const std::size_t from = index > 80 ? index - 80 : 0;
  std::fprintf(stderr,
    "  %s: canonical state differs at byte %zu (sizes %zu vs %zu)\n"
    "    before: ...%s...\n"
    "    after:  ...%s...\n",
    label, index, before.size(), after.size(),
    before.substr(from, 160).c_str(), after.substr(from, 160).c_str());
}

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
  for(const Construction* ac : a.map.constructions) {
    const Construction* bc = nullptr;
    for(const Construction* candidate : b.map.constructions) {
      if(candidate->point == ac->point) {
        bc = candidate;
        break;
      }
    }
    if(!bc) continue;
    check("construction group", ac->constructionGroup->group,
      bc->constructionGroup->group);
    check("construction flags", ac->flags, bc->flags);
    for(int stuff = STUFF_INIT; stuff < STUFF_COUNT; stuff++) {
      check("construction commodity", ac->commodityCount[stuff],
        bc->commodityCount[stuff]);
      check("construction production", ac->commodityProd[stuff],
        bc->commodityProd[stuff]);
      check("construction previous production", ac->commodityProdPrev[stuff],
        bc->commodityProdPrev[stuff]);
    }
    // FEAT-04: ledger fields
    check("ledger purchase_cost", ac->purchase_cost, bc->purchase_cost);
    check("ledger capital_invested", ac->capital_invested, bc->capital_invested);
    check("ledger maintenance", ac->maintenance, bc->maintenance);
    check("ledger operation_cost", ac->operation_cost, bc->operation_cost);
    check("ledger revenue", ac->revenue, bc->revenue);
  }
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
      if(serializeWorldGameplayState(*world)
        != serializeWorldGameplayState(*reloaded)) {
        std::fprintf(stderr,
          "  canonical state differs after round-trip: %s\n", name.c_str());
        ok = false;
      }
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
  TCHECK_EQ(serializeWorldGameplayState(*world),
    serializeWorldGameplayState(*reloaded));
  std::error_code ec;
  std::filesystem::remove(tmp, ec);
}

TTEST(continuation_after_load_preserves_rng) {
  // TST-08: the save must restore the random stream after map construction
  // during load, so future simulation steps match the uninterrupted world.
  BasicUrbg::get().reseed(424242);
  city_settings city;
  city.with_village = true;
  city.without_trees = false;
  std::unique_ptr<World> world =
    new_city(&city, getConfig()->worldSize.get());
  TCHECK(world != nullptr);
  if(!world)
    return;

  for(int day = 0; day < 200; day++)
    world->do_time_step();

  const std::filesystem::path tmp =
    headless::user_data() / "continuation_rng.scn.gz";
  std::error_code ec;
  std::filesystem::remove(tmp, ec);
  world->save(tmp);
  const std::string saved_rng_state = BasicUrbg::get().state();

  std::unique_ptr<World> reloaded = World::load(tmp);
  TCHECK(reloaded != nullptr);
  if(reloaded) {
    TCHECK_EQ(BasicUrbg::get().state(), saved_rng_state);
    TCHECK(compare_worlds(*world, *reloaded, "continuation_rng_before"));
    std::string next_rng_state = saved_rng_state;
    bool continuation_ok = true;
    for(int day = 0; day < 200; day++) {
      TCHECK(BasicUrbg::get().restoreState(next_rng_state));
      world->do_time_step();
      const std::string after_world = BasicUrbg::get().state();
      TCHECK(BasicUrbg::get().restoreState(next_rng_state));
      reloaded->do_time_step();
      if(!compare_worlds(*world, *reloaded, "continuation_rng_day")) {
        std::fprintf(stderr, "  continuation diverged on day %d\n", day + 1);
        continuation_ok = false;
        break;
      }
      next_rng_state = after_world;
    }
    TCHECK(continuation_ok);
  }
  std::filesystem::remove(tmp, ec);
}

TTEST(round_trip_port_fractional_remainder) {
  // BUG-16: pence is the sub-unit carried into the next port update. It is
  // not derived from the visible totals and must survive a save/load.
  std::unique_ptr<World> world = std::make_unique<World>(100);
  auto* port = new Port(*world, &portConstructionGroup);
  port->pence = 73;
  const MapPoint point(10, 10);
  port->place(point);

  const std::filesystem::path tmp =
    headless::user_data() / "roundtrip_port_pence.scn.gz";
  std::error_code ec;
  std::filesystem::remove(tmp, ec);
  world->save(tmp);

  std::unique_ptr<World> reloaded = World::load(tmp);
  TCHECK(reloaded != nullptr);
  if(reloaded) {
    auto* loaded = dynamic_cast<Port*>(reloaded->map(point)->construction);
    TCHECK(loaded != nullptr);
    if(loaded)
      TCHECK_EQ(loaded->pence, 73);
  }
  std::filesystem::remove(tmp, ec);
}

TTEST(round_trip_2143_preserves_simulation_protocol) {
  // REF-07 Fase 5: protocol, seed and version are part of the save contract.
  city_settings city;
  city.with_village = true;
  city.without_trees = false;
  constexpr std::uint64_t seed = 0x5eed123456789abcULL;
  std::unique_ptr<World> world =
    new_city(&city, 100, SimulationProtocol::DETERMINISTIC_V1, seed);
  TCHECK(world != nullptr);
  if(!world)
    return;
  for(int day = 0; day < 120; day++)
    world->do_time_step();

  const StateFingerprint128 before =
    captureStateSnapshot(*world).fingerprint();
  const std::string canonicalBefore = serializeWorldGameplayState(*world);

  const std::filesystem::path tmp =
    headless::user_data() / "roundtrip_v1.scn.gz";
  std::error_code ec;
  std::filesystem::remove(tmp, ec);
  world->save(tmp);

  std::unique_ptr<World> reloaded = World::load(tmp);
  TCHECK(reloaded != nullptr);
  if(reloaded) {
    TCHECK(reloaded->simulationProtocol()
      == SimulationProtocol::DETERMINISTIC_V1);
    TCHECK_EQ(reloaded->simulationSeed(), seed);
    const std::string canonicalAfter = serializeWorldGameplayState(*reloaded);
    if(canonicalAfter != canonicalBefore)
      report_canonical_difference(canonicalBefore, canonicalAfter, "v1");
    TCHECK_EQ(canonicalAfter, canonicalBefore);
    TCHECK(captureStateSnapshot(*reloaded).fingerprint() == before);
    TCHECK(compare_worlds(*world, *reloaded, "roundtrip_v1"));
  }
  std::filesystem::remove(tmp, ec);
}

TTEST(compat_2142_fixture_loads_as_legacy) {
  // REF-07 Fase 5 gate: a genuine 2142 save (pre-bump writer) must load with
  // the Legacy protocol and zeroed seed, and stay deterministic.
  const std::filesystem::path fixture =
    std::filesystem::path(LINCITYNG_TEST_FIXTURES_DIR)
    / "compat" / "compat-2142-basic.scn.gz";

  std::unique_ptr<World> first = World::load(fixture);
  TCHECK(first != nullptr);
  if(!first)
    return;
  TCHECK(first->simulationProtocol() == SimulationProtocol::LEGACY);
  TCHECK_EQ(first->simulationSeed(), std::uint64_t(0));

  std::unique_ptr<World> second = World::load(fixture);
  TCHECK(second != nullptr);
  if(second) {
    TCHECK(captureStateSnapshot(*first).fingerprint()
      == captureStateSnapshot(*second).fingerprint());
    std::string rng = BasicUrbg::get().state();
    bool lockstep = true;
    for(int day = 0; day < 30 && lockstep; day++) {
      TCHECK(BasicUrbg::get().restoreState(rng));
      first->do_time_step();
      const std::string after = BasicUrbg::get().state();
      TCHECK(BasicUrbg::get().restoreState(rng));
      second->do_time_step();
      lockstep = captureStateSnapshot(*first).fingerprint()
        == captureStateSnapshot(*second).fingerprint();
      rng = after;
    }
    TCHECK(lockstep);
  }

  // The loaded Legacy save must round-trip through the 2143 writer too.
  const std::filesystem::path tmp =
    headless::user_data() / "compat_2142_roundtrip.scn.gz";
  std::error_code ec;
  std::filesystem::remove(tmp, ec);
  first->save(tmp);
  std::unique_ptr<World> reloaded = World::load(tmp);
  TCHECK(reloaded != nullptr);
  if(reloaded) {
    TCHECK(reloaded->simulationProtocol() == SimulationProtocol::LEGACY);
    TCHECK(compare_worlds(*first, *reloaded, "compat_2142"));
    TCHECK_EQ(serializeWorldGameplayState(*first),
      serializeWorldGameplayState(*reloaded));
  }
  std::filesystem::remove(tmp, ec);
}

TTEST(continuation_after_load_preserves_v1_trajectory) {
  // REF-07 Fase 5 gate: a 2143 v1 save must continue exactly for 200 days.
  city_settings city{true, false};
  constexpr std::uint64_t seed = 0xabcdef1234567890ULL;
  std::unique_ptr<World> world =
    new_city(&city, 100, SimulationProtocol::DETERMINISTIC_V1, seed);
  TCHECK(world != nullptr);
  if(!world)
    return;
  for(int day = 0; day < 200; day++)
    world->do_time_step();

  const std::filesystem::path tmp =
    headless::user_data() / "continuation_v1.scn.gz";
  std::error_code ec;
  std::filesystem::remove(tmp, ec);
  world->save(tmp);

  std::unique_ptr<World> reloaded = World::load(tmp);
  TCHECK(reloaded != nullptr);
  if(reloaded) {
    TCHECK(reloaded->simulationProtocol()
      == SimulationProtocol::DETERMINISTIC_V1);
    TCHECK_EQ(reloaded->simulationSeed(), seed);
    bool ok = captureStateSnapshot(*world).fingerprint()
      == captureStateSnapshot(*reloaded).fingerprint();
    for(int day = 0; day < 200 && ok; day++) {
      world->do_time_step();
      reloaded->do_time_step();
      ok = captureStateSnapshot(*world).fingerprint()
        == captureStateSnapshot(*reloaded).fingerprint();
    }
    TCHECK(ok);
  }
  std::filesystem::remove(tmp, ec);
}

TTEST(save_load_crosses_period_boundaries) {
  // REF-07 Fase 5 gate: save/load on the monthly (99/100) and yearly
  // (1199/1200) boundaries must not change the following trajectory.
  const int boundaries[] = {99, 100, 1199, 1200};
  for(const int boundary : boundaries) {
    city_settings city{true, false};
    std::unique_ptr<World> world = new_city(&city, 100,
      SimulationProtocol::DETERMINISTIC_V1, 424242);
    TCHECK(world != nullptr);
    if(!world)
      return;
    for(int day = 0; day < boundary; day++)
      world->do_time_step();
    TCHECK_EQ(world->total_time, boundary);

    const std::filesystem::path tmp = headless::user_data()
      / ("boundary_" + std::to_string(boundary) + ".scn.gz");
    std::error_code ec;
    std::filesystem::remove(tmp, ec);
    world->save(tmp);

    std::unique_ptr<World> reloaded = World::load(tmp);
    TCHECK(reloaded != nullptr);
    if(!reloaded) {
      std::filesystem::remove(tmp, ec);
      continue;
    }
    bool ok = captureStateSnapshot(*world).fingerprint()
      == captureStateSnapshot(*reloaded).fingerprint();
    for(int day = 0; day < 3 && ok; day++) {
      world->do_time_step();
      reloaded->do_time_step();
      ok = captureStateSnapshot(*world).fingerprint()
        == captureStateSnapshot(*reloaded).fingerprint();
      if(!ok) {
        const StateSnapshot left = captureStateSnapshot(*world);
        const StateSnapshot right = captureStateSnapshot(*reloaded);
        if(const auto difference = left.firstDifference(right)) {
          std::fprintf(stderr,
            "  boundary %d diverged after %d day(s): section %s byte %zu\n",
            boundary, day + 1, difference->section.c_str(),
            difference->offset);
        }
        report_canonical_difference(serializeWorldGameplayState(*world),
          serializeWorldGameplayState(*reloaded), "boundary");
      }
    }
    TCHECK(ok);
    std::filesystem::remove(tmp, ec);
  }
}

} // namespace

int main(int argc, char** argv) {
  std::filesystem::path appData = LINCITYNG_TEST_DATA_DIR;
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
    else if(arg == "--filter")
      tiny_test::set_filter(next());
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
    headless::init_env(appData, userData, seed);
    rc = tiny_test::run_all_tests();
    headless::shutdown_env();
  } catch(const std::exception& err) {
    std::fprintf(stderr, "savetest bootstrap failed: %s\n", err.what());
    return 1;
  }
  return rc;
}
