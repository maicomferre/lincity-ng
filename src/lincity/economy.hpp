/* ---------------------------------------------------------------------- *
 * src/lincity/economy.hpp
 * This file is part of Lincity-NG.
 *
 * Copyright (C) 2026      Maicom Ferreira <maicomferre3242@gmail.com>
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

/* Pure economic formulas in one place (R2). Everything that computes a
 * cost, a tax or a clamp goes through these; the yearly update and the
 * constructions call them, and the unit tests cover them directly.
 * See .devdocs/09-mapa-economia-taxas.md for the canonical mapping. */

#ifndef LINCITY_ECONOMY_HPP
#define LINCITY_ECONOMY_HPP

#include <algorithm>          // for min, max

#include "all_buildings.hpp"  // for INTEREST_RATE
#include "lin-city.hpp"       // for MAX_TECH_LEVEL

/* Construction cost scaled by tech level (ConstructionGroup::getCosts). */
inline int compute_cost(int cost, int cost_mul, int tech) {
  return static_cast<int>(cost *
    (1.0f + cost_mul * tech / static_cast<float>(MAX_TECH_LEVEL)));
}

/* Maintenance cost scaled by tech level (health/fire station). */
inline int compute_maintenance_cost(int base, int mul, int tech) {
  return base * (1 + mul * tech / MAX_TECH_LEVEL);
}

/* Annual taxes collected on labor/coal/ore/goods delivery. */
inline int compute_income_tax(int labor, int rate) {
  return labor * rate / 100;
}

inline int compute_coal_tax(int units, int rate) {
  return units * rate / 10;
}

inline int compute_ore_tax(int units, int rate) {
  return units * rate / 10;
}

/* Goods tax plus a tech bonus on the tax itself (float on purpose:
 * the bonus only shows at very high tech levels). */
inline int compute_goods_tax(int goods, int rate, int tech) {
  int tax = goods * rate / 100;
  tax += (int)((float)(tax * rate) * (float)tech / 2000000.0);
  return tax;
}

/* Progressive export discount: the world-market price drops as you
 * export more, so the tax take shrinks. */
inline int compute_export_discount(int trade_ex) {
  int discount = 0;
  for(int trigger : {25000, 50000, 100000, 200000, 400000, 800000})
    discount += std::max(0, trade_ex - trigger) / 10;
  return discount;
}

/* Annual interest on debt, capped at 1e6. */
inline int compute_interest(int total_money) {
  if(total_money >= 0)
    return 0;
  return std::min((-total_money / 1000) * INTEREST_RATE, 1000000);
}

/* Engine-wide money clamp. */
inline int compute_clamp_money(int money) {
  if(money > 2000000000)
    return 2000000000;
  if(money < -2000000000)
    return -2000000000;
  return money;
}

/* Regressive daily school cost by occupancy: busy is the number of days
 * the school operated in the previous 100 days (0..100). Empty schools
 * cost the state the most per pupil: 2x running cost at 0%, 1x at 50%
 * and 0.5x from 75% occupancy up. */
inline int compute_school_cost(int busy, int running_cost) {
  double load = busy / 50.0;                  // 0..2
  double factor = 2.0 - std::min(1.5, load);  // 2.0 down to 0.5
  return static_cast<int>(running_cost * factor);
}

#endif // LINCITY_ECONOMY_HPP
