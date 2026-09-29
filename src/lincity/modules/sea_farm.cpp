/* ---------------------------------------------------------------------- *
 * src/lincity/modules/sea_farm.cpp
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

#include "sea_farm.hpp"

#include <libxml++/parsers/textreader.h>
#include <libxml/xmlwriter.h>
#include <cstdlib>
#include <string>

#include "lincity-ng/Mps.hpp"
#include "lincity/MapPoint.hpp"
#include "lincity/groups.hpp"
#include "lincity/lin-city.hpp"
#include "lincity/messages.hpp"
#include "lincity/world.hpp"
#include "util/xmlutil.hpp"
#include "util/gettextutil.hpp"

SeaFarmConstructionGroup seaFarmConstructionGroup(
    N_("Sea Farm"),
    N_("Sea Farms"),
    FALSE,
    GROUP_SEA_FARM,
    GROUP_SEA_FARM_SIZE,
    GROUP_SEA_FARM_COLOUR,
    GROUP_SEA_FARM_COST_MUL,
    GROUP_SEA_FARM_BUL_COST,
    GROUP_SEA_FARM_FIREC,
    GROUP_SEA_FARM_COST,
    GROUP_SEA_FARM_TECH,
    GROUP_SEA_FARM_RANGE
);

Construction *SeaFarmConstructionGroup::createConstruction(World& world) {
  return new SeaFarm(world, this);
}

bool
SeaFarmConstructionGroup::can_build_here(const World& world,
  const MapPoint point, Message::ptr& message
) const {
  /* Sea farm must fit within map bounds and every tile must be
   * water (lake/river/pond) with no covering construction.
   * Note: is_bare() returns false for water, so we check
   * group==GROUP_WATER && reportingConstruction==nullptr. */
  for(int j = 0; j < size; j++) {
    for(int i = 0; i < size; i++) {
      MapPoint pt = point.e(i).s(j);
      if(!world.map.is_visible(pt)) {
        message = OutsideMapMessage::create(point);
        return false;
      }
      const MapTile* tile = world.map(pt);
      if(tile->group != GROUP_WATER || tile->reportingConstruction) {
        auto* self = const_cast<SeaFarmConstructionGroup*>(this);
        message = CannotBuildHereMessage::create(*self, point, message);
        return false;
      }
    }
  }
  return true;
}

SeaFarm::SeaFarm(World& world, ConstructionGroup *cstgrp) :
  Construction(world)
{
  this->constructionGroup = cstgrp;
  this->tech = world.tech_level;
  this->food_this_month = 0;
  this->goods_this_month = 0;
  initialize_commodities();

  commodityMaxCons[STUFF_LABOR] = 100 * SEA_FARM_LABOR;
  commodityMaxCons[STUFF_LOVOLT] = 100 * SEA_FARM_POWER;
  commodityMaxCons[STUFF_WATER] = 100 * SEA_FARM_WATER;
}

void
SeaFarm::place(MapPoint point) {
  Construction::place(point);
}

void
SeaFarm::update() {
  /* Consume labor, water, energy */
  int used_labor = (SEA_FARM_LABOR < commodityCount[STUFF_LABOR]
    ? SEA_FARM_LABOR : commodityCount[STUFF_LABOR]);

  if(used_labor <= 0) return;

  int used_power = (SEA_FARM_POWER < commodityCount[STUFF_LOVOLT]
    ? SEA_FARM_POWER : commodityCount[STUFF_LOVOLT]);

  if(used_power <= 0) return;

  int used_water = (SEA_FARM_WATER < commodityCount[STUFF_WATER]
    ? SEA_FARM_WATER : commodityCount[STUFF_WATER]);

  if(used_water <= 0) return;

  consumeStuff(STUFF_LABOR, used_labor);
  consumeStuff(STUFF_LOVOLT, used_power);
  consumeStuff(STUFF_WATER, used_water);

  /* Water pollution reduces production */
  int water_pol = world.map(point)->ground.water_pol;
  double pollution_factor = 1.0 - water_pol / (double)WATER_POL_MAX;
  if(pollution_factor < 0.0) pollution_factor = 0.0;

  /* Produce food and goods proportional to inputs used */
  int foodprod = (int)(SEA_FARM_FOOD * used_labor / SEA_FARM_LABOR
                 * pollution_factor);
  int goodsprod = (int)(SEA_FARM_GOODS * used_labor / SEA_FARM_LABOR
                 * pollution_factor);

  if(foodprod > 0) {
    produceStuff(STUFF_FOOD, foodprod);
    food_this_month += foodprod;
  }
  if(goodsprod > 0) {
    produceStuff(STUFF_GOODS, goodsprod);
    goods_this_month += goodsprod;
  }

  /* Record ledger entries */
  recordExpense(SEA_FARM_POWER);
  recordRevenue(foodprod + goodsprod);
}

void
SeaFarm::report(Mps& mps, bool production) const {
  mps.add_sdd(N_("Sea Farm"), point.x, point.y);
  mps.addBlank();
  if(production) {
    mps.add_ss(N_("Food"), num_to_ansi(food_this_month));
    mps.add_ss(N_("Goods"), num_to_ansi(goods_this_month));
  }
  mps.add_ss(N_("Labor"), num_to_ansi(commodityCount[STUFF_LABOR]));
  mps.add_ss(N_("Power"), num_to_ansi(commodityCount[STUFF_LOVOLT]));
  mps.add_ss(N_("Water"), num_to_ansi(commodityCount[STUFF_WATER]));
}

void
SeaFarm::animate(unsigned long) {
}

void
SeaFarm::save(xmlTextWriterPtr xmlWriter) const {
  Construction::save(xmlWriter);
  xmlTextWriterWriteElement(xmlWriter, (xmlStr)"food_this_month",
    xmlFormat<int>(food_this_month));
  xmlTextWriterWriteElement(xmlWriter, (xmlStr)"goods_this_month",
    xmlFormat<int>(goods_this_month));
  xmlTextWriterWriteElement(xmlWriter, (xmlStr)"tech",
    xmlFormat<int>(tech));
}

bool
SeaFarm::loadMember(xmlpp::TextReader& xmlReader, unsigned int ldsv_version) {
  std::string name = xmlReader.get_name();
  if(name == "food_this_month") {
    food_this_month = xmlParse<int>(xmlReader.read_inner_xml());
  }
  else if(name == "goods_this_month") {
    goods_this_month = xmlParse<int>(xmlReader.read_inner_xml());
  }
  else if(name == "tech") {
    tech = xmlParse<int>(xmlReader.read_inner_xml());
  }
  else return Construction::loadMember(xmlReader, ldsv_version);
  return true;
}
