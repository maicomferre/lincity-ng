/* ---------------------------------------------------------------------- *
 * src/lincity/all_buildings.h
 * This file is part of Lincity-NG.
 *
 * Copyright (C) 1995-1997 I J Peters
 * Copyright (C) 1997-2005 Greg Sharp
 * Copyright (C) 2000-2004 Corey Keasling
 * Copyright (C) 2022-2024 David Bears <dbear4q@gmail.com>
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

#ifndef __all_buildings_h__
#define __all_buildings_h__

#include "lin-city.hpp"

/************* Buildings stuff *********************/

#define DAYS_PER_POLLUTION      14
#define RAIL_POLLUTION          1
#define DAYS_PER_RAIL_POLLUTION 30
#define ROAD_POLLUTION          1
#define DAYS_PER_ROAD_POLLUTION 20
#define UNNAT_DEATHS_COST       500

/* FEAT-06: water pollution */
#define WATER_POL_MAX         1000
#define PORT_WATER_POLLUTION     3
#define OILWELL_WATER_POLLUTION  5
#define REFINERY_WATER_POLLUTION 8
#define HEAVY_IND_WATER_POLLUTION 6

#define INCOME_TAX_RATE 8
#define COAL_TAX_RATE 15
#define ORE_TAX_RATE 15
#define GOODS_TAX_RATE 1
#define DOLE_RATE 15
#define TRANSPORT_COST_RATE 14
#define IM_PORT_COST_RATE 1

#define DAYS_BETWEEN_COVER (NUMOF_DAYS_IN_MONTH*3)
#define COVER_TOLERANCE_DAYS 5

#define ORE_RESERVE       1000
#define COAL_RESERVE_SIZE 10000
#define COAL_PER_RESERVE  1000
#define OIL_RESERVE       1000
#define NUMOF_OIL_RESERVES ((map.len() * map.len()) / 500)

#define LABOR_LOAD_COAL  18
#define LABOR_LOAD_ORE    9
#define LABOR_LOAD_STEEL 15

#define OILWELL_COST      800
#define OILWELL_COST_MUL  8
#define OILWELL_BUL_COST  100000
#define OILWELL_TECH     (10 * (MAX_TECH_LEVEL / 1000))
#define OILWELL_FIREC     10
#define OILWELL_RANGE     0
#define OILWELL_SIZE      3

#define OILWELL_LABOR   150
#define MAX_LABOR_AT_OILWELL (20 * OILWELL_LABOR)
#define PETROLEUM_PER_WELL 5
#define MIN_OIL_FOR_WELL  OIL_RESERVE

#define REFINERY_COST      1200
#define REFINERY_COST_MUL  6
#define REFINERY_BUL_COST  200000
#define REFINERY_TECH     (20 * (MAX_TECH_LEVEL / 1000))
#define REFINERY_FIREC     5
#define REFINERY_RANGE     0
#define REFINERY_SIZE      3

#define REFINERY_LABOR   200
#define MAX_LABOR_AT_REFINERY (20 * REFINERY_LABOR)
#define GOODS_PER_REFINERY 3
#define MAX_PETROLEUM_AT_REFINERY 10000
#define MAX_GOODS_AT_REFINERY 2000

/* FEAT-05: Sea Farm */
#define SEA_FARM_COST      15000
#define SEA_FARM_COST_MUL  30
#define SEA_FARM_BUL_COST  200000
#define SEA_FARM_TECH      (40 * (MAX_TECH_LEVEL / 1000))
#define SEA_FARM_FIREC     5
#define SEA_FARM_RANGE     0
#define SEA_FARM_SIZE      4

#define SEA_FARM_LABOR     180
#define MAX_LABOR_AT_SEA_FARM (20 * SEA_FARM_LABOR)
#define SEA_FARM_POWER     200
#define MAX_POWER_AT_SEA_FARM (20 * SEA_FARM_POWER)
#define SEA_FARM_WATER     10
#define MAX_WATER_AT_SEA_FARM (20 * SEA_FARM_WATER)
#define SEA_FARM_FOOD      350
#define SEA_FARM_GOODS     8
#define MAX_FOOD_AT_SEA_FARM (20 * SEA_FARM_FOOD)
#define MAX_GOODS_AT_SEA_FARM (20 * SEA_FARM_GOODS)

//used on markets, transport, shanties when burning waste
#define WASTE_BURN_TIME 350
#define WASTE_BURN_UNCONTROLLED .001

/* interest rate *10  ie 10 is 1% */
#define INTEREST_RATE 15

/******* Buildings // GROUPS ************/
#define red(x) (32 + x)
#define green(x) (64 + x)
#define yellow(x) (96 + x)
#define blue(x) (128 + x)
#define magenta(x) (160 + x)
#define cyan(x) (192 + x)
#define white(x) (224 + x)

#define GROUP_NAME_LEN   20

#define GROUP_BARE_COLOUR  (green(12))
#define GROUP_BARE_COST    0
#define GROUP_BARE_COST_MUL 1
#define GROUP_BARE_BUL_COST 1
#define GROUP_BARE_TECH    (0 * (MAX_TECH_LEVEL / 1000))
#define GROUP_BARE_FIREC   0

#define GROUP_WATER_COLOUR    (blue(31))
#define GROUP_WATER_COST      1000000
#define GROUP_WATER_COST_MUL 2
#define GROUP_WATER_BUL_COST      1000000
#define GROUP_WATER_TECH      (0 * (MAX_TECH_LEVEL / 1000))
#define GROUP_WATER_FIREC 0

#define GROUP_BURNT_COLOUR    (red(30))
#define GROUP_BURNT_COST   0    /* Unused */
#define GROUP_BURNT_COST_MUL   1        /* Unused */
#define GROUP_BURNT_BUL_COST  1000
#define GROUP_BURNT_TECH   0    /* Unused */
#define GROUP_BURNT_FIREC  0

#define GROUP_USED_COLOUR (green(12))   /* Unused */
#define GROUP_USED_COST   0     /* Unused */
#define GROUP_USED_COST_MUL  1  /* Unused */
#define GROUP_USED_BUL_COST  0  /* Unused */
#define GROUP_USED_TECH   0     /* Unused */
#define GROUP_USED_FIREC  0     /* Unused */

#define GROUP_DESERT_COLOUR  (yellow(18))
#define GROUP_DESERT_COST    0
#define GROUP_DESERT_COST_MUL 1
#define GROUP_DESERT_BUL_COST 1
#define GROUP_DESERT_TECH    (0 * (MAX_TECH_LEVEL / 1000))
#define GROUP_DESERT_FIREC   0

#define GROUP_TREE_COLOUR  (green(12))
#define GROUP_TREE_COST    0
#define GROUP_TREE_COST_MUL 1
#define GROUP_TREE_BUL_COST 1
#define GROUP_TREE_TECH    (0 * (MAX_TECH_LEVEL / 1000))
#define GROUP_TREE_FIREC   0

#define GROUP_TREE2_COLOUR  (green(12))
#define GROUP_TREE2_COST    0
#define GROUP_TREE2_COST_MUL 1
#define GROUP_TREE2_BUL_COST 1
#define GROUP_TREE2_TECH    0
#define GROUP_TREE2_FIREC   0

#define GROUP_TREE3_COLOUR  (green(12))
#define GROUP_TREE3_COST    0
#define GROUP_TREE3_COST_MUL 1
#define GROUP_TREE3_BUL_COST 1
#define GROUP_TREE3_TECH    0
#define GROUP_TREE3_FIREC   0

#endif // __all_buildings_h__

/** @file lincity/all_buildings.h */
