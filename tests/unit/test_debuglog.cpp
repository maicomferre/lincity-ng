/* Layer 1 tests for the development logging facility (util/debuglog).
 * The facility reads LINCITYNG_LOG_* env vars on init; each test resets
 * the env state via _debuglog_test_reset() so the following init re-reads
 * the environment it just set. */

#include "../tiny_test.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "util/debuglog.hpp"

namespace {

/* helper: reset, set one env var, init, return state */
void reset_and_init() {
  lincity::log::_debuglog_test_reset();
  lincity::log::init_logging();
}

} // namespace

TTEST(debuglog_defaults_quiet) {
  // no env vars set (reset clears state): only warn/error enabled
  unsetenv("LINCITYNG_LOG_LEVEL");
  unsetenv("LINCITYNG_LOG_AREAS");
  reset_and_init();

  using namespace lincity::log;
  TCHECK(enabled(kSim, kWarn));
  TCHECK(enabled(kSim, kError));
  TCHECK(!enabled(kSim, kInfo));
  TCHECK(!enabled(kSim, kDebug));
  TCHECK(!enabled(kSim, kTrace));
}

TTEST(debuglog_global_level_env) {
  setenv("LINCITYNG_LOG_LEVEL", "debug", 1);
  unsetenv("LINCITYNG_LOG_AREAS");
  reset_and_init();

  using namespace lincity::log;
  // global level applies to every area
  TCHECK(enabled(kSim, kDebug));
  TCHECK(enabled(kEcon, kDebug));
  TCHECK(enabled(kSave, kInfo));
  TCHECK(!enabled(kSim, kTrace)); // trace is below the debug threshold
  TCHECK(enabled(kSim, kInfo));
  TCHECK(enabled(kSim, kWarn));
}

TTEST(debuglog_per_area_override) {
  setenv("LINCITYNG_LOG_LEVEL", "warn", 1);
  setenv("LINCITYNG_LOG_AREAS", "sim=debug,vehicles=trace", 1);
  reset_and_init();

  using namespace lincity::log;
  // sim raised to debug (trace still below; info and up enabled)
  TCHECK(enabled(kSim, kDebug));
  TCHECK(!enabled(kSim, kTrace));
  TCHECK(enabled(kSim, kInfo));
  // vehicles raised to trace (everything on)
  TCHECK(enabled(kVehicles, kTrace));
  // untouched areas stay at the global warn
  TCHECK(!enabled(kEcon, kInfo));
  TCHECK(enabled(kEcon, kWarn));
  TCHECK(!enabled(kSave, kDebug));
}

TTEST(debuglog_invalid_areas_are_ignored) {
  setenv("LINCITYNG_LOG_AREAS", "nope=trace,sim=bogus,frames", 1);
  setenv("LINCITYNG_LOG_LEVEL", "warn", 1);
  reset_and_init();

  using namespace lincity::log;
  // unknown area name: ignored
  TCHECK(!enabled(kEcon, kDebug));
  // unknown level for sim: ignored
  TCHECK(!enabled(kSim, kDebug));
  // "frames" without '=': ignored
  TCHECK(!enabled(kFrames, kTrace));
}

TTEST(debuglog_names) {
  using namespace lincity::log;
  TCHECK_EQ(std::string("sim"), area_name(kSim));
  TCHECK_EQ(std::string("econ"), area_name(kEcon));
  TCHECK_EQ(std::string("save"), area_name(kSave));
  TCHECK_EQ(std::string("vehicles"), area_name(kVehicles));
  TCHECK_EQ(std::string("frames"), area_name(kFrames));
  TCHECK_EQ(std::string("config"), area_name(kConfig));
  TCHECK_EQ(std::string("test"), area_name(kTest));
  TCHECK_EQ(std::string("general"), area_name(kGeneral));
  TCHECK_EQ(std::string("TRACE"), level_name(kTrace));
  TCHECK_EQ(std::string("DEBUG"), level_name(kDebug));
  TCHECK_EQ(std::string("INFO"), level_name(kInfo));
  TCHECK_EQ(std::string("WARN"), level_name(kWarn));
  TCHECK_EQ(std::string("ERROR"), level_name(kError));
  TCHECK_EQ(std::string("?"), area_name(static_cast<Area>(-1)));
  TCHECK_EQ(std::string("?"), area_name(kAreaCount));
  TCHECK_EQ(std::string("?"), level_name(static_cast<Level>(-1)));
  TCHECK_EQ(std::string("?"), level_name(kLevelCount));
}

TTEST(debuglog_out_of_range_never_enabled) {
  reset_and_init();
  using namespace lincity::log;
  TCHECK(!enabled(static_cast<Area>(-1), kWarn));
  TCHECK(!enabled(kAreaCount, kWarn));
  TCHECK(!enabled(kSim, static_cast<Level>(-1)));
  TCHECK(!enabled(kSim, kLevelCount));
}

TTEST(debuglog_writes_to_file_sink) {
  // real end-to-end: env-config the file sink + level, log a line, read back.
  // Uses the log_line() API (always available) rather than the LNG_LOG macro,
  // which is a no-op in builds compiled without LINCITYNG_ENABLE_DEBUG_LOG.
  const char* path = "/tmp/lincity-ng-test-debuglog.log";
  std::remove(path);
  setenv("LINCITYNG_LOG_FILE", path, 1);
  setenv("LINCITYNG_LOG_LEVEL", "debug", 1);
  setenv("LINCITYNG_LOG_TIMESTAMP", "0", 1);
  reset_and_init();

  lincity::log::log_line(lincity::log::kSim, lincity::log::kDebug,
    "test_debuglog.cpp", 1, fmt::format("hello {} {}", 42, "world"));
  lincity::log::log_line(lincity::log::kSim, lincity::log::kTrace,
    "test_debuglog.cpp", 2, "should not appear");

  FILE* f = std::fopen(path, "r");
  TCHECK(f != nullptr);
  if(!f)
    return;
  char buf[512];
  std::string first, second;
  if(std::fgets(buf, sizeof(buf), f))
    first = buf;
  if(std::fgets(buf, sizeof(buf), f))
    second = buf;
  std::fclose(f);
  std::remove(path);

  // timestamp disabled, so the line starts with the area tag
  TCHECK(first.find("[sim]") == 0);
  TCHECK(first.find("DEBUG") != std::string::npos);
  TCHECK(first.find("hello 42 world") != std::string::npos);
  TCHECK(first.find("should not appear") == std::string::npos);
  TCHECK(second.empty()); // trace was filtered out -> only one line
}
