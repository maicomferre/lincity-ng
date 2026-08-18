/* ---------------------------------------------------------------------- *
 * src/util/debuglog.cpp
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

#include "debuglog.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>

namespace lincity {
namespace log {

namespace {

const char* const kAreaNames[kAreaCount] = {
  "sim", "econ", "save", "vehicles", "frames", "config", "test", "general"
};
const char* const kLevelNames[kLevelCount] = {
  "TRACE", "DEBUG", "INFO", "WARN", "ERROR"
};

struct AreaLevels {
  int v[kAreaCount];
  AreaLevels() {
    for(int i = 0; i < kAreaCount; ++i)
      v[i] = kWarn; // quiet by default: only warnings/errors show
  }
};

/* Static defaults are kWarn so a log call before init never floods output;
 * init_logging() re-parses the env vars on top of these. Manual once-guard
 * (atomic + mutex) instead of std::call_once so the test hook can reset. */
AreaLevels g_area_levels;
bool g_initialized = false;
std::mutex g_init_mutex;
std::mutex g_log_mutex;
FILE* g_file = nullptr;   // null => stderr
bool g_show_timestamp = true;

/* Parses "trace|debug|info|warn|error" (case-insensitive). Returns false
 * for anything else so the caller can keep the previous value. */
bool parse_level(const char* text, Level& out) {
  if(!text)
    return false;
  const char* names[kLevelCount] = {
    "trace", "debug", "info", "warn", "error"
  };
  for(int i = 0; i < kLevelCount; ++i) {
    if(strcmp(text, names[i]) == 0) {
      out = static_cast<Level>(i);
      return true;
    }
  }
  return false;
}

/* Parses one "area=level" or "area" token; without "=" the area goes to
 * the global level. Unknown areas/levels are ignored (defaults stay). */
void parse_area_token(const char* token) {
  const char* eq = strchr(token, '=');
  const std::string area(token, eq ? (size_t)(eq - token) : strlen(token));
  Level level = kWarn;
  if(!eq || !parse_level(eq + 1, level))
    return; // unknown level for this token: leave defaults
  for(int i = 0; i < kAreaCount; ++i) {
    if(area == kAreaNames[i]) {
      g_area_levels.v[i] = level;
      return;
    }
  }
}

/* Reads env vars over the kWarn defaults. Caller holds g_init_mutex. */
void do_init() {
  if(const char* level = getenv("LINCITYNG_LOG_LEVEL")) {
    Level parsed;
    if(parse_level(level, parsed)) {
      for(int i = 0; i < kAreaCount; ++i)
        g_area_levels.v[i] = parsed;
    }
  }

  if(const char* areas = getenv("LINCITYNG_LOG_AREAS")) {
    char* buf = strdup(areas);
    if(buf) {
      for(char* token = strtok(buf, ","); token; token = strtok(nullptr, ","))
        parse_area_token(token);
      free(buf);
    }
  }

  if(const char* path = getenv("LINCITYNG_LOG_FILE")) {
    if(*path)
      g_file = fopen(path, "a");
  }

  if(const char* ts = getenv("LINCITYNG_LOG_TIMESTAMP")) {
    if(strcmp(ts, "0") == 0)
      g_show_timestamp = false;
  }
}

} // namespace

void init_logging() {
  std::lock_guard<std::mutex> lock(g_init_mutex);
  if(!g_initialized) {
    do_init();
    g_initialized = true;
  }
}

bool enabled(Area area, Level level) {
  if(area < 0 || area >= kAreaCount)
    return false;
  if(level < 0 || level >= kLevelCount)
    return false;
  return level >= g_area_levels.v[area];
}

const char* area_name(Area area) {
  return area >= 0 && area < kAreaCount ? kAreaNames[area] : "?";
}

const char* level_name(Level level) {
  return level >= 0 && level < kLevelCount ? kLevelNames[level] : "?";
}

void log_line(Area area, Level level, const char* file, int line,
  const std::string& message) {
  if(!enabled(area, level))
    return;
  std::lock_guard<std::mutex> lock(g_log_mutex);
  FILE* out = g_file ? g_file : stderr;

  std::string timestamp;
  if(g_show_timestamp) {
    const auto now = std::chrono::system_clock::now();
    const std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    localtime_r(&tt, &tm_buf);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", &tm_buf);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      now.time_since_epoch()) % 1000;
    timestamp = fmt::format("[{}.{:03d}] ", ts, ms.count());
  }

  fmt::print(out, "{}[{}] {} {}:{} {}\n",
    timestamp, area_name(area), level_name(level), file, line, message);
  fflush(out);
}

/* Test hook: forget the parsed env state so a following init_logging()
 * re-reads the environment. Not used by the game or the harnesses. */
void _debuglog_test_reset() {
  std::lock_guard<std::mutex> lock(g_init_mutex);
  g_initialized = false;
  g_file = nullptr;
  g_show_timestamp = true;
  for(int i = 0; i < kAreaCount; ++i)
    g_area_levels.v[i] = kWarn;
}

} // namespace log
} // namespace lincity
