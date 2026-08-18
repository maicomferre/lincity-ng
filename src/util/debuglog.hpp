/* ---------------------------------------------------------------------- *
 * src/util/debuglog.hpp
 * This file is part of Lincity-NG.
 *
 * Copyright (C) 2026      <fork>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
** ---------------------------------------------------------------------- */

#ifndef LINCITYNG_UTIL_DEBUGLOG_HPP__
#define LINCITYNG_UTIL_DEBUGLOG_HPP__

/* Development debugging facility (fork). NOT a game feature: logging is
 * read-only and never touches simulation state, RNG or saves, so it cannot
 * affect determinism (R9). Only compiled when LINCITYNG_ENABLE_DEBUG_LOG
 * is defined (see .devdocs/10 "Modo de debugging"); otherwise every LNG_LOG
 * is a no-op.
 *
 * Control at runtime (env vars, no recompile needed):
 *   LINCITYNG_LOG_LEVEL    global level: trace|debug|info|warn|error (default: warn)
 *   LINCITYNG_LOG_AREAS    per-area overrides, e.g. "sim=debug,vehicles=trace"
 *   LINCITYNG_LOG_FILE     append to a file instead of stderr
 *   LINCITYNG_LOG_TIMESTAMP=0  drop the [timestamp] prefix (cleaner diffs)
 *
 * Areas are per-subsystem so an agent can target one area cheaply:
 * sim/save/vehicles/frames are instrumented; econ is trace-only on the
 * income/expense hot path. FUTURE: add a `gui` area (src/lincity-ng:
 * GameView, Dialog, MiniMap, Mps) in a later session — the enum below and
 * this header's area table in debuglog.cpp are the only places to extend.
 */

#include <fmt/format.h>

#include <cstdint>
#include <string>

namespace lincity {
namespace log {

enum Area : int {
  kSim,       // simulate.cpp: do_time_step, monthly/yearly accounting
  kEcon,      // engine.cpp: income/expense hot path (trace only)
  kSave,      // xmlloadsave.cpp: load/save milestones and ldsv_version
  kVehicles,  // Vehicles.cpp: spawn/move/reap/delete this -> markDead
  kFrames,    // world.cpp: ExtraFrame add/remove/move + canary asserts
  kConfig,    // Config.cpp: option/env parsing decisions
  kTest,      // test harnesses: tiny_test, simtest, savetest
  kGeneral,   // anything not covered by a specific area
  kAreaCount
};

enum Level : int {
  kTrace = 0,
  kDebug,
  kInfo,
  kWarn,
  kError,
  kLevelCount
};

/* Reads the LINCITYNG_LOG_* env vars once (first call wins; safe to call
 * from every binary entry point). Does nothing if logging is compiled out. */
void init_logging();

/* true when `level` is loud enough for `area` under the current config. */
bool enabled(Area area, Level level);

/* Names for the manifest/tooling; return "?" for invalid enum values. */
const char* area_name(Area area);
const char* level_name(Level level);

/* Writes one line to the sink: "[ts] [area] LEVEL file:line message". */
void log_line(Area area, Level level, const char* file, int line,
  const std::string& message);

/* Test hook (unit tests only): resets the env state so the next
 * init_logging() re-reads LINCITYNG_LOG_*. Not called by the game. */
void _debuglog_test_reset();

} // namespace log
} // namespace lincity

#ifdef LINCITYNG_ENABLE_DEBUG_LOG
#define LNG_LOG(area, level, ...) \
  do { \
    if(::lincity::log::enabled(area, level)) \
      ::lincity::log::log_line(area, level, __FILE__, __LINE__, \
        ::fmt::format(__VA_ARGS__)); \
  } while(0)
#else
#define LNG_LOG(area, level, ...) ((void)0)
#endif

#endif
