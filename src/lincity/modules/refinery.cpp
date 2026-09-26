/* ---------------------------------------------------------------------- *
 * src/lincity/modules/refinery.cpp
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

#include "refinery.hpp"

#include <libxml++/parsers/textreader.h>
#include <libxml/xmlwriter.h>
#include <string>

#include "lincity-ng/Mps.hpp"
#include "lincity/MapPoint.hpp"
#include "lincity/groups.hpp"
#include "lincity/lin-city.hpp"
#include "lincity/stats.hpp"
#include "lincity/world.hpp"
#include "util/xmlutil.hpp"
#include "util/gettextutil.hpp"

RefineryConstructionGroup refineryConstructionGroup(
     N_("Refinery"),
     N_("Refineries"),
     FALSE,
     GROUP_REFINERY,
     GROUP_REFINERY_SIZE,
     GROUP_REFINERY_COLOUR,
     GROUP_REFINERY_COST_MUL,
     GROUP_REFINERY_BUL_COST,
     GROUP_REFINERY_FIREC,
     GROUP_REFINERY_COST,
     GROUP_REFINERY_TECH,
     GROUP_REFINERY_RANGE
);

Construction *RefineryConstructionGroup::createConstruction(World& world) {
  return new Refinery(world, this);
}

bool
RefineryConstructionGroup::can_build_here(const World& world,
  const MapPoint point, Message::ptr& message
) const {
  return ConstructionGroup::can_build_here(world, point, message);
}

Refinery::Refinery(World& world, ConstructionGroup *cstgrp) :
  Construction(world)
{
  this->constructionGroup = cstgrp;
  this->anim = 0;
  this->animate_enable = false;
  this->working_days = 0;
  this->busy = 0;
  this->anim_count = 0;
  this->total_processed = 0;
  initialize_commodities();

  commodityMaxCons[STUFF_PETROLEUM] = 100 * PETROLEUM_PER_WELL;
  commodityMaxProd[STUFF_GOODS] = 100 * GOODS_PER_REFINERY;
  commodityMaxCons[STUFF_LABOR] = 100 * REFINERY_LABOR;
}

void Refinery::update()
{
    animate_enable = false;

    if ((commodityCount[STUFF_PETROLEUM] >= PETROLEUM_PER_WELL)
    && (commodityCount[STUFF_LABOR] >= REFINERY_LABOR))
    {
      consumeStuff(STUFF_PETROLEUM, PETROLEUM_PER_WELL);
      produceStuff(STUFF_GOODS, GOODS_PER_REFINERY);
      consumeStuff(STUFF_LABOR, REFINERY_LABOR);
      total_processed += PETROLEUM_PER_WELL;
      animate_enable = true;
      working_days++;
    }

    if (world.total_time % 100 == 99) {
        reset_prod_counters();
        busy = working_days;
        working_days = 0;
    }
}

void Refinery::animate(unsigned long real_time) {
  int& frame = frameIt->frame;
  if(animate_enable && real_time >= anim) {
    anim = real_time + 200;
    if(anim_count < 8)
      frame = anim_count;
    else if (anim_count < 12)
      frame = 14 - anim_count;
    else
      frame = 16 - anim_count;
    if(++anim_count == 16)
      anim_count = 0;
  }
}

void Refinery::report(Mps& mps, bool production) const {
  mps.add_s(constructionGroup->name);
  mps.add_sfp(N_("busy"), busy);
  mps.add_sd(N_("Processed"), total_processed);
  list_commodities(mps, production);
}

void
Refinery::place(MapPoint point) {
  Construction::place(point);
  this->total_processed = 0;
}

void Refinery::bulldoze() {
  Construction::bulldoze();
}

void Refinery::save(xmlTextWriterPtr xmlWriter) const {
  xmlTextWriterWriteElement(xmlWriter, (xmlStr)"total_processed",
    xmlFormat<int>(total_processed));
  Construction::save(xmlWriter);
}

bool Refinery::loadMember(xmlpp::TextReader& xmlReader, unsigned int ldsv_version) {
  std::string tag = xmlReader.get_name();
  if(tag == "total_processed") {
    total_processed = xmlParse<int>(xmlReader.read_inner_xml());
    return true;
  }
  return Construction::loadMember(xmlReader, ldsv_version);
}
