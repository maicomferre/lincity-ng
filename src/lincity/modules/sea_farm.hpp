/* ---------------------------------------------------------------------- *
 * src/lincity/modules/sea_farm.hpp
 * FEAT-05: Sea Farm — builds on water tiles, consumes labor+water+energy,
 * produces food+goods. Inspired by upstream #339.
 *
 * This file is part of Lincity-NG.
 *
 * Copyright (C) 2022-2025 David Bears <[EMAIL]>
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

#ifndef __LINCITYNG_LINCITY_MODULES_SEAFARM_HPP__
#define __LINCITYNG_LINCITY_MODULES_SEAFARM_HPP__

#include <array>                      // for array
#include <string>                     // for string

#include "lincity/all_buildings.hpp"
#include "lincity/commodities.hpp"    // for CommodityRule, Commodity
#include "lincity/lin-city.hpp"       // for MAX_TECH_LEVEL
#include "lincity/lintypes.hpp"       // for ConstructionGroup, Construction
#include "lincity/util.hpp"           // for num_to_ansi

class World;

#define GROUP_SEA_FARM_COLOUR (blue(20))
#define GROUP_SEA_FARM_COST SEA_FARM_COST
#define GROUP_SEA_FARM_COST_MUL SEA_FARM_COST_MUL
#define GROUP_SEA_FARM_BUL_COST SEA_FARM_BUL_COST
#define GROUP_SEA_FARM_TECH SEA_FARM_TECH
#define GROUP_SEA_FARM_FIREC SEA_FARM_FIREC
#define GROUP_SEA_FARM_RANGE SEA_FARM_RANGE
#define GROUP_SEA_FARM_SIZE SEA_FARM_SIZE

class SeaFarmConstructionGroup: public ConstructionGroup {
public:
    SeaFarmConstructionGroup(
        const std::string& name,
        const std::string& name_plural,
        unsigned short no_credit,
        unsigned short group,
        unsigned short size, int colour,
        int cost_mul, int bul_cost, int fire_chance,
        int cost, int tech, int range
    ): ConstructionGroup(
        name, name_plural, no_credit, group, size, colour, cost_mul, bul_cost,
        fire_chance,   cost, tech, range
    ) {
        commodityRuleCount[STUFF_FOOD].maxload = MAX_FOOD_AT_SEA_FARM;
        commodityRuleCount[STUFF_FOOD].take = false;
        commodityRuleCount[STUFF_FOOD].give = true;
        commodityRuleCount[STUFF_GOODS].maxload = MAX_GOODS_AT_SEA_FARM;
        commodityRuleCount[STUFF_GOODS].take = false;
        commodityRuleCount[STUFF_GOODS].give = true;
        commodityRuleCount[STUFF_LOVOLT].maxload = MAX_POWER_AT_SEA_FARM;
        commodityRuleCount[STUFF_LOVOLT].take = true;
        commodityRuleCount[STUFF_LOVOLT].give = false;
        commodityRuleCount[STUFF_LABOR].maxload = MAX_LABOR_AT_SEA_FARM;
        commodityRuleCount[STUFF_LABOR].take = true;
        commodityRuleCount[STUFF_LABOR].give = false;
        commodityRuleCount[STUFF_WATER].maxload = MAX_WATER_AT_SEA_FARM;
        commodityRuleCount[STUFF_WATER].give = false;
        commodityRuleCount[STUFF_WATER].take = true;
    }
    virtual bool can_build_here(const World& world,
      const MapPoint point, Message::ptr& message) const override;
    virtual Construction *createConstruction(World& world) override;
    virtual bool buildOnWater() const override { return true; }
};

extern SeaFarmConstructionGroup seaFarmConstructionGroup;

class SeaFarm: public Construction {
public:
    SeaFarm(World& world, ConstructionGroup *cstgrp);

    virtual ~SeaFarm() { }
    virtual void update() override;
    virtual void report(Mps& mps, bool production) const override;
    virtual void animate(unsigned long real_time) override;
    virtual void place(MapPoint point) override;

    virtual void save(xmlTextWriterPtr xmlWriter) const override;
    virtual bool loadMember(xmlpp::TextReader& xmlReader, unsigned int ldsv_version) override;

    int  food_this_month;
    int  goods_this_month;
    int  tech;
};

#endif // __LINCITYNG_LINCITY_MODULES_SEAFARM_HPP__
