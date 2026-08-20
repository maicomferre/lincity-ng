/* ---------------------------------------------------------------------- *
 * src/util/randutil.cpp
 * This file is part of Lincity-NG.
 *
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

#include "randutil.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <time.h>
#include <random>
#include <vector>

BasicUrbg::BasicUrbg() {
  std::vector<std::random_device::result_type> entropy(9);
  std::generate(entropy.begin(), entropy.begin() + 8, std::random_device());
  entropy[8] = time(nullptr);
  std::seed_seq seed(entropy.begin(), entropy.end());
  base_urbg.seed(seed);
}

BasicUrbg::~BasicUrbg() = default;

BasicUrbg&
BasicUrbg::get() {
  static thread_local BasicUrbg instance;
  return instance;
}

void
BasicUrbg::reseed(result_type seed) {
  base_urbg.seed(seed);
}

std::string
BasicUrbg::state() const {
  std::ostringstream output;
  output << base_urbg;
  return output.str();
}

bool
BasicUrbg::restoreState(const std::string& serialized) {
  base_engine candidate;
  std::istringstream input(serialized);
  if(!(input >> candidate))
    return false;
  base_urbg = candidate;
  return true;
}

BasicUrbg::result_type
BasicUrbg::operator()() {
  return base_urbg();
}

int
lincityRand() {
  constexpr uint64_t rand_range = static_cast<uint64_t>(RAND_MAX) + 1;
  return static_cast<int>(BasicUrbg::get()() % rand_range);
}
