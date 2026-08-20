/* ---------------------------------------------------------------------- *
 * src/lincity-ng/MainLincity.cpp
 * This file is part of Lincity-NG.
 *
 * Copyright (C) 2005      David Kamphausen <david.kamphausen@web.de>
 * Copyright (C) 2025      David Bears <dbear4q@gmail.com>
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

#include "MainLincity.hpp"

#include <stdlib.h>                       // for time-compatible C types
#include <time.h>                         // for time
#include <iostream>                       // for char_traits, basic_ostream
#include <string_view>                    // for string_view
#include <stdexcept>                      // for runtime_error
#include <string>                         // for basic_string, operator+
#include <fmt/format.h>
#include <fmt/std.h> // IWYU pragma: keep

#include "TimerInterface.hpp"             // for reset_start_time
#include "gui/DialogBuilder.hpp"          // for DialogBuilder
#include "lincity/init_game.hpp"          // for city_settings, new_city
#include "lincity/lin-city.hpp"             // for SIM_DELAY_SLOW
#include "lincity/modules/all_modules.hpp"  // for initializeModules
#include "lincity/world.hpp"                // for World
#include "util/gettextutil.hpp"
#include "util/randutil.hpp"                 // for BasicUrbg

extern void init_types(void);
extern void initFactories(void);

int simDelay = SIM_DELAY_SLOW;
/******************************************/

namespace {

bool saveCityAtomically(const World& world,
    const std::filesystem::path& filename, std::string_view operation) {
  std::filesystem::path tmp = filename;
  tmp += ".tmp";
  try {
    world.save(tmp);
    std::error_code ec;
    std::filesystem::rename(tmp, filename, ec);
    if(ec) {
      std::cerr << "error: failed to commit " << operation << " to '"
        << filename.string() << "': " << ec.message() << std::endl;
      std::filesystem::remove(tmp, ec);
      return false;
    }
    std::cout << operation << " game to '" << filename.string() << "'"
      << std::endl;
    return true;
  } catch(const std::exception& err) {
    std::cerr << "error: failed to " << operation << " to '"
      << filename.string() << "': " << err.what() << std::endl;
    std::error_code ec;
    std::filesystem::remove(tmp, ec);
    return false;
  } catch(...) {
    std::cerr << "error: failed to " << operation << " to '"
      << filename.string() << "': unknown exception" << std::endl;
    std::error_code ec;
    std::filesystem::remove(tmp, ec);
    return false;
  }
}

} // namespace

void setSimulationDelay( int speed )
{
    simDelay = speed;
}

/*
 * get Data form Lincity NG and Save City
 */
bool saveCityNG(const World& world, const std::filesystem::path& filename) {
  return saveCityAtomically(world, filename, "save");
}

/*
 * Autosave City to File.
 * Writes to a temporary file first and renames it into place, so that a
 * crash mid-write never corrupts an existing save.
 */
bool autoSaveCityNG(const World& world, const std::filesystem::path& filename) {
  return saveCityAtomically(world, filename, "autosave");
}

/*
 * Load City and do setup for Lincity NG.
 */
std::unique_ptr<World> loadCityNG(const std::filesystem::path& filename) {
  std::unique_ptr<World> world;
  try {
    world = World::load(filename);
    std::cout << "loaded game from " << filename << std::endl;
  } catch(std::runtime_error& err) {
    std::cerr << "error: failed to load game from " << filename
      << ": " << err.what() << std::endl;
    DialogBuilder()
      .titleText(_("Error"))
      .messageAddTextBold(_("Error: Failed to load game."))
      .messageAddText(fmt::format(_("Could not load {}."), filename))
      .messageAddText(err.what())
      .imageFile("images/gui/dialogs/error.png")
      .buttonSet(DialogBuilder::ButtonSet::OK)
      .build();
  }

  return world;
}

/*
 * Continue policy: the clean-exit save wins over the autosave; without
 * either, start a fresh city.
 */
std::unique_ptr<World> loadContinueCityNG(
    const std::filesystem::path& userDataDir, int worldSize) {
  std::filesystem::path file = userDataDir / "9_currentGameNG.scn.gz";
  if(std::filesystem::exists(file))
    return loadCityNG(file);

  std::filesystem::path autosave = userDataDir / "autosave.scn.gz";
  if(std::filesystem::exists(autosave))
    return loadCityNG(autosave);

  city_settings city;
  city.with_village = true;
  city.without_trees = false;
  return new_city(&city, worldSize);
}

void initLincity()
{
    /* Set up the paths to certain files and directories */
    // init_path_strings();

    /* Make sure the save directory exists */
    // check_savedir();

    /*initialize Desktop Componenet Factories*/
    initFactories();

    /* Initialize random number generator */
    BasicUrbg::get().reseed(static_cast<BasicUrbg::result_type>(time(0)));

    //mps_init(); //CK no implemented

    // initialize constructions
    initializeModules();

    // animation time
    reset_start_time ();

}


/** @file lincity-ng/MainLincity.cpp */
