/* ---------------------------------------------------------------------- *
 * src/lincity/util.hpp
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

#ifndef __LINCITYNG_LINCITY_UTIL_HPP__
#define __LINCITYNG_LINCITY_UTIL_HPP__

#include <string>

#include <clocale>      // for localeconv

#ifdef NDEBUG
#define used_in_assert maybe_unused
#else
#define used_in_assert
#endif

// TODO: move to NG
const char *current_month(int current_time);
/* Re-translate the cached month names (call after changing the language). */
void retranslate_months();
int current_year(int current_time);
//void format_number5(char *str, int num);
//void format_pos_number4(char *str, int num);
//void format_power(char *str, size_t size, long power);
std::string num_to_ansi(long num);

/* Thousands grouping with a fixed separator (pure, locale-independent,
 * deterministic — tested in the unit suite). 1234567 -> "1'234'567". */
std::string format_thousands(long num, char sep);

/* Thousands grouping using the current C locale's thousands separator
 * (falls back to apostrophe when the locale has none). */
std::string format_money(long num);

#endif // __LINCITYNG_LINCITY_UTIL_HPP__

/** @file lincity/lclib.h */
