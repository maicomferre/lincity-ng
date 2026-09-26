/* ---------------------------------------------------------------------- *
 * src/lincity/modules/refinery.hpp
 * This file is part of Lincity-NG.
 *
 * Copyright (C) 2025 Maicom Ferreira
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

#ifndef __LINCITYNG_LINCITY_MODULES_REFINERY_HPP__
#define __LINCITYNG_LINCITY_MODULES_REFINERY_HPP__

#include <array>
#include <string>

#include "lincity/all_buildings.hpp"
#include "lincity/commodities.hpp"
#include "lincity/lin-city.hpp"
#include "lincity/lintypes.hpp"
#include "lincity/messages.hpp"

class World;

#define GROUP_REFINERY_COLOUR (yellow(18))
#define GROUP_REFINERY_COST      REFINERY_COST
#define GROUP_REFINERY_COST_MUL  REFINERY_COST_MUL
#define GROUP_REFINERY_BUL_COST  REFINERY_BUL_COST
#define GROUP_REFINERY_TECH      REFINERY_TECH
#define GROUP_REFINERY_FIREC     REFINERY_FIREC
#define GROUP_REFINERY_RANGE     REFINERY_RANGE
#define GROUP_REFINERY_SIZE      REFINERY_SIZE

class RefineryConstructionGroup: public ConstructionGroup {
public:
    RefineryConstructionGroup(
        const std::string& name,
        const std::string& name_plural,
        unsigned short no_credit,
        unsigned short group,
        unsigned short size, int colour,
        int cost_mul, int bul_cost, int fire_chance,
        int cost, int tech, int range
    ): ConstructionGroup(
        name, name_plural, no_credit, group, size, colour, cost_mul, bul_cost,
        fire_chance, cost, tech, range
    ) {
        commodityRuleCount[STUFF_LABOR].maxload = MAX_LABOR_AT_REFINERY;
        commodityRuleCount[STUFF_LABOR].take = true;
        commodityRuleCount[STUFF_LABOR].give = false;
        commodityRuleCount[STUFF_PETROLEUM].maxload = MAX_PETROLEUM_AT_REFINERY;
        commodityRuleCount[STUFF_PETROLEUM].take = true;
        commodityRuleCount[STUFF_PETROLEUM].give = false;
        commodityRuleCount[STUFF_GOODS].maxload = MAX_GOODS_AT_REFINERY;
        commodityRuleCount[STUFF_GOODS].take = false;
        commodityRuleCount[STUFF_GOODS].give = true;
    }
    virtual Construction *createConstruction(World& world) override;
    virtual bool can_build_here(const World& world, const MapPoint point,
      Message::ptr& message) const override;
};

extern RefineryConstructionGroup refineryConstructionGroup;

class Refinery: public Construction {
public:
    Refinery(World& world, ConstructionGroup *cstgrp);
    virtual ~Refinery() {}
    virtual void update() override;
    virtual void report(Mps& mps, bool production) const override;
    virtual void animate(unsigned long real_time) override;
    virtual void place(MapPoint point) override;
    virtual void bulldoze() override;

    virtual void save(xmlTextWriterPtr xmlWriter) const override;
    virtual bool loadMember(xmlpp::TextReader& xmlReader, unsigned int ldsv_version) override;

    int total_processed;
    unsigned long anim;
    bool animate_enable;
    int working_days, busy;
    unsigned int anim_count;
};

#endif // __LINCITYNG_LINCITY_MODULES_REFINERY_HPP__
