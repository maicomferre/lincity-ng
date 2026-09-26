/* ---------------------------------------------------------------------- *
 * src/lincity/modules/oilwell.hpp
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

#ifndef __LINCITYNG_LINCITY_MODULES_OILWELL_HPP__
#define __LINCITYNG_LINCITY_MODULES_OILWELL_HPP__

#include <array>
#include <string>

#include "lincity/all_buildings.hpp"
#include "lincity/commodities.hpp"
#include "lincity/lin-city.hpp"
#include "lincity/lintypes.hpp"
#include "lincity/messages.hpp"

class World;

#define GROUP_OILWELL_COLOUR (red(18))
#define GROUP_OILWELL_COST      OILWELL_COST
#define GROUP_OILWELL_COST_MUL  OILWELL_COST_MUL
#define GROUP_OILWELL_BUL_COST  OILWELL_BUL_COST
#define GROUP_OILWELL_TECH      OILWELL_TECH
#define GROUP_OILWELL_FIREC     OILWELL_FIREC
#define GROUP_OILWELL_RANGE     OILWELL_RANGE
#define GROUP_OILWELL_SIZE      OILWELL_SIZE

class OilwellConstructionGroup: public ConstructionGroup {
public:
    OilwellConstructionGroup(
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
        commodityRuleCount[STUFF_LABOR].maxload = MAX_LABOR_AT_OILWELL;
        commodityRuleCount[STUFF_LABOR].take = true;
        commodityRuleCount[STUFF_LABOR].give = false;
        commodityRuleCount[STUFF_PETROLEUM].maxload = MAX_PETROLEUM_AT_REFINERY;
        commodityRuleCount[STUFF_PETROLEUM].take = false;
        commodityRuleCount[STUFF_PETROLEUM].give = true;
    }
    virtual Construction *createConstruction(World& world) override;
    virtual bool can_build_here(const World& world, const MapPoint point,
      Message::ptr& message) const override;
};

extern OilwellConstructionGroup oilwellConstructionGroup;

class Oilwell: public Construction {
public:
    Oilwell(World& world, ConstructionGroup *cstgrp);
    virtual ~Oilwell() {}
    virtual void update() override;
    virtual void report(Mps& mps, bool production) const override;
    virtual void animate(unsigned long real_time) override;
    virtual void place(MapPoint point) override;
    virtual void bulldoze() override;

    virtual void save(xmlTextWriterPtr xmlWriter) const override;
    virtual bool loadMember(xmlpp::TextReader& xmlReader, unsigned int ldsv_version) override;

    void makeLake();

    int total_extracted;
    unsigned long anim;
    bool animate_enable;
    int working_days, busy;
    unsigned int anim_count;
};

#endif // __LINCITYNG_LINCITY_MODULES_OILWELL_HPP__
