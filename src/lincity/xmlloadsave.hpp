/* ---------------------------------------------------------------------- *
 * src/lincity/xmlloadsave.hpp
 * This file is part of Lincity-NG.
 *
 * Copyright (C) 2024-2025 David Bears <dbear4q@gmail.com>
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

#ifndef __xmlloadsave_h__
#define __xmlloadsave_h__

#include <string>

class World;

#define LOADSAVE_VERSION_CURRENT 2145
#define LOADSAVE_VERSION_COMPAT 2130

std::string serializeWorldGameplayState(const World& world);

// 2143:
// - persist the simulation protocol/seed/version (REF-07), the pollution
//   queue and the remaining trajectory state of Coalmine, Commune, Market,
//   School, Windmill and Windpower; 2142 saves load as Legacy with seed 0
// 2142:
// - persist the Legacy rng_state (TST-08)
// 2141:
// - persist the Port fractional remainder (BUG-16)
// 2140:
// - updated to be consistent with the new World structure
// 2130:
// - migrated to libxml++

#endif /* __xmlloadsave_h__ */

/** @file lincity/loadsave.h */
