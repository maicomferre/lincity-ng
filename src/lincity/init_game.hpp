/* ---------------------------------------------------------------------- *
 * src/lincity/init_game.hpp
 * This file is part of Lincity-NG.
 *
 * Copyright (C) 1995-1997 I J Peters
 * Copyright (C) 1997-2005 Greg Sharp
 * Copyright (C) 2000-2004 Corey Keasling
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

#ifndef __init_game_h__
#define __init_game_h__

#include <memory>

#include "world.hpp"

typedef struct _CitySettings city_settings;

struct _CitySettings {
  /* BUG-48: sem estes initializers, `city_settings city;` deixaria os
   * dois bool indeterminados, e create_new_city() lê without_trees sem
   * validar. Três testes faziam exatamente isso e o UBSan pegou:
   * "load of value 213, which is not a valid value for type 'bool'".
   * Com os initializers, `city_settings city;` vale false e nenhum
   * chamador pode errar de novo. C++17 mantém isto como agregado, então
   * os `city_settings{true, false}` dos outros testes seguem válendo. */
  bool with_village = false;
  bool without_trees = false;
};

std::unique_ptr<World>
create_new_city(city_settings *city, int mapSize,
  int old_setup_ground, int climate,
  SimulationProtocol protocol = SimulationProtocol::LEGACY,
  std::uint64_t simulationSeed = 0);

std::unique_ptr<World>
new_city(city_settings *city, int mapSize,
  SimulationProtocol protocol = SimulationProtocol::LEGACY,
  std::uint64_t simulationSeed = 0);

std::unique_ptr<World>
new_desert_city(city_settings *city, int mapSize,
  SimulationProtocol protocol = SimulationProtocol::LEGACY,
  std::uint64_t simulationSeed = 0);

std::unique_ptr<World>
new_temperate_city(city_settings *city, int mapSize,
  SimulationProtocol protocol = SimulationProtocol::LEGACY,
  std::uint64_t simulationSeed = 0
);

std::unique_ptr<World>
new_swamp_city(city_settings *city, int mapSize,
  SimulationProtocol protocol = SimulationProtocol::LEGACY,
  std::uint64_t simulationSeed = 0);

#endif /* __init_game_h__ */

/** @file lincity/init_game.h */
