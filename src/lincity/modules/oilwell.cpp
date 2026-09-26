/* ---------------------------------------------------------------------- *
 * src/lincity/modules/oilwell.cpp
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

#include "oilwell.hpp"

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

OilwellConstructionGroup oilwellConstructionGroup(
     N_("Oil Well"),
     N_("Oil Wells"),
     FALSE,
     GROUP_OILWELL,
     GROUP_OILWELL_SIZE,
     GROUP_OILWELL_COLOUR,
     GROUP_OILWELL_COST_MUL,
     GROUP_OILWELL_BUL_COST,
     GROUP_OILWELL_FIREC,
     GROUP_OILWELL_COST,
     GROUP_OILWELL_TECH,
     GROUP_OILWELL_RANGE
);

Construction *OilwellConstructionGroup::createConstruction(World& world) {
  return new Oilwell(world, this);
}

bool
OilwellConstructionGroup::can_build_here(const World& world,
  const MapPoint point, Message::ptr& message
) const {
  if(!ConstructionGroup::can_build_here(world, point, message)) return false;

  int oil = 0;
  for(int i = 0; i < size; i++)
  for(int j = 0; j < size; j++)
    oil += world.map(point.e(j).s(i))->oil_reserve;
  if(oil < MIN_OIL_FOR_WELL) {
    message = NoOreMessage::create();
    return false;
  }

  return true;
}

Oilwell::Oilwell(World& world, ConstructionGroup *cstgrp) :
  Construction(world)
{
  this->constructionGroup = cstgrp;
  this->anim = 0;
  this->animate_enable = false;
  this->working_days = 0;
  this->busy = 0;
  this->anim_count = 0;
  this->total_extracted = 0;
  initialize_commodities();

  commodityMaxProd[STUFF_PETROLEUM] = 100 * PETROLEUM_PER_WELL;
  commodityMaxCons[STUFF_PETROLEUM] = 100 * PETROLEUM_PER_WELL;
  commodityMaxCons[STUFF_LABOR] = 100 * OILWELL_LABOR;
}

void Oilwell::update()
{
    animate_enable = false;

    if ((total_extracted < 99999)
    && (commodityCount[STUFF_LABOR] >= OILWELL_LABOR))
    {
      for(MapPoint p(point); p.y < point.y + constructionGroup->size; p.y++)
      for(p.x = point.x; p.x < point.x + constructionGroup->size; p.x++) {
        if(world.map(p)->oil_reserve > 0) {
          world.map(p)->oil_reserve--;
          total_extracted++;
          produceStuff(STUFF_PETROLEUM, PETROLEUM_PER_WELL);
          consumeStuff(STUFF_LABOR, OILWELL_LABOR);
          animate_enable = true;
          working_days++;
          goto end_extraction;
        }
      }
    }
    end_extraction:

    if (world.total_time % 100 == 99) {
        reset_prod_counters();
        busy = working_days;
        working_days = 0;
    }

    bool any_oil = false;
    for(MapPoint p(point); p.y < point.y + constructionGroup->size; p.y++)
    for(p.x = point.x; p.x < point.x + constructionGroup->size; p.x++)
      if(world.map(p)->oil_reserve > 0) { any_oil = true; goto check_lake; }
    check_lake:
    if(!any_oil && total_extracted > 0
      && commodityCount[STUFF_LABOR] == 0
      && commodityCount[STUFF_PETROLEUM] == 0
    ) {
      detach();
      makeLake();
      return;
    }
}

void Oilwell::animate(unsigned long real_time) {
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

void Oilwell::report(Mps& mps, bool production) const {
  mps.add_s(constructionGroup->name);
  mps.add_sfp(N_("busy"), busy);
  mps.add_sd(N_("Extracted"), total_extracted);
  list_commodities(mps, production);
}

void
Oilwell::place(MapPoint point) {
  Construction::place(point);

  int oil = 0;
  for(MapPoint p(point); p.y < point.y + constructionGroup->size; p.y++)
  for(p.x = point.x; p.x < point.x + constructionGroup->size; p.x++)
    oil += world.map(p)->oil_reserve;
  if(oil < 1)
    oil = 1;
  this->total_extracted = 0;
}

void Oilwell::bulldoze() {
  Construction::bulldoze();
}

void
Oilwell::makeLake() {
  int size = constructionGroup->size;
  for(MapPoint p(point); p.y < point.y + size; p.y++)
  for(p.x = point.x; p.x < point.x + constructionGroup->size; p.x++) {
    MapTile& tile = *world.map(p);
    if(tile.oil_reserve < OIL_RESERVE / 2) {
      tile.setTerrain(GROUP_WATER);
      tile.flags |= FLAG_HAS_UNDERGROUND_WATER;
    }
  }
}

void Oilwell::save(xmlTextWriterPtr xmlWriter) const {
  xmlTextWriterWriteElement(xmlWriter, (xmlStr)"total_extracted",
    xmlFormat<int>(total_extracted));
  Construction::save(xmlWriter);
}

bool Oilwell::loadMember(xmlpp::TextReader& xmlReader, unsigned int ldsv_version) {
  std::string tag = xmlReader.get_name();
  if(tag == "total_extracted") {
    total_extracted = xmlParse<int>(xmlReader.read_inner_xml());
    return true;
  }
  return Construction::loadMember(xmlReader, ldsv_version);
}
