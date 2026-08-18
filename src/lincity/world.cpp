/* ---------------------------------------------------------------------- *
 * src/lincity/world.cpp
 * This file is part of Lincity-NG.
 *
 * Copyright (C) 1995-1997 I J Peters
 * Copyright (C) 1997-2005 Greg Sharp
 * Copyright (C) 2000-2004 Corey Keasling
 * Copyright (C) 2005      Matthias Braun <matze@braunis.de>
 * Copyright (C) 2022-2025 David Bears <dbear4q@gmail.com>
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

#include "world.hpp"

#include <stdlib.h>            // for NULL
#include <cassert>             // for assert
#include <initializer_list>    // for initializer_list
#include <iostream>            // for basic_ostream, operator<<, basic_ostre...
#include <iterator>            // for advance
#include <map>                 // for map
#include <string>              // for basic_string, char_traits, operator<

#include "all_buildings.hpp"   // for GROUP_WATER_COST
#include "groups.hpp"          // for GROUP_WATER, GROUP_BARE, GROUP_DESERT
#include "lin-city.hpp"        // for FLAG_IS_RIVER, FLAG_TRANSPARENT, FLAG_...
#include "lintypes.hpp"        // for Construction, ConstructionGroup
#include "lincity-ng/Config.hpp"  // for getConfig
#include "modules/market.hpp"  // for Market
#include "modules/tile.hpp"    // for TileConstructionGroup, desertConstruct...
#include "modules/track_road_rail.hpp"  // for transport_group_at
#include "resources.hpp"       // for ExtraFrame, ResourceGroup
#include "Vehicles.hpp"        // for Vehicle (complete type needed by World::~World)
#include "util/debuglog.hpp"   // for LNG_LOG

#ifndef NDEBUG
namespace {
/* Debug-only registry of every live ExtraFrame node (addFrame/removeFrame/
 * ~MapTile maintain it). Global, not per-Map: MapTile has no back-pointer
 * and several maps can be alive at once; splice never changes addresses,
 * so moveFrameTo does not touch it. */
std::unordered_set<const ExtraFrame*> g_live_frames;

void register_frame(const ExtraFrame* frame)
{
    assert(g_live_frames.insert(frame).second
      && "register_frame: address already registered");
}
void unregister_frame(const ExtraFrame* frame)
{
    assert(g_live_frames.erase(frame) == 1
      && "removeFrame: frame is not a registered live frame "
      "(double kill of an already removed frame?)");
}

/* O(n) debug scan: does this tile's list own the node `it` points to?
 * Compares node addresses, never iterators of different lists. */
bool owns_frame(const std::list<ExtraFrame>& frames,
    const std::list<ExtraFrame>::iterator& it)
{
    const ExtraFrame* frame = &*it;
    for(const ExtraFrame& candidate : frames)
    {
        if(&candidate == frame)
        {   return true;}
    }
    return false;
}
} // namespace
#endif

//BUG-12: deterministic set order — see ConstructionPointLess in world.hpp
bool ConstructionPointLess::operator()(const Construction* a,
  const Construction* b) const
{
  const MapPoint& pa = a->point;
  const MapPoint& pb = b->point;
  return pa.x < pb.x || (pa.x == pb.x && pa.y < pb.y);
}

Ground::Ground() {
  altitude = 0;
  ecotable = 0;
  wastes = 0;
  pollution = 0;
  water_alt = 0;
  water_pol = 0;
  water_wast = 0;
  water_next = 0;
  int1 = 0;
  int2 = 0;
  int3 = 0;
  int4 = 0;
}
Ground::~Ground() {}

MapTile::MapTile(MapPoint point) :
  point(point), ground()
{
    construction = NULL;
    reportingConstruction = NULL;
    flags = 0;
    type = 0;
    group = GROUP_BARE;
    pollution = 0;
    ore_reserve = 0;
    coal_reserve = 0;
    pollution = 0;
}

MapTile::MapTile(MapTile&& other) noexcept :
  point(other.point), ground(other.ground),
  construction(other.construction),
  reportingConstruction(other.reportingConstruction),
  type(other.type), group(other.group), flags(other.flags),
  coal_reserve(other.coal_reserve), ore_reserve(other.ore_reserve),
  pollution(other.pollution),
  framesptr(std::move(other.framesptr))
{
    //Ownership of the construction and the frame list moves with the tile;
    //null the pointers so the moved-from tile's destructor frees nothing.
    other.construction = nullptr;
    other.reportingConstruction = nullptr;
}

MapTile::~MapTile()
{
    //Here the order matters: the Construction is deleted first (its
    //detach() may still remove frames from this tile's list, which stays
    //alive until the unique_ptr destroys it right after this body).
    if(construction)
    {   delete construction;}
#ifndef NDEBUG
    //teardown is not a kill: unregister silently
    if(framesptr)
    {
        for(const ExtraFrame& frame : *framesptr)
        {   g_live_frames.erase(&frame);}
    }
#endif
}

void MapTile::setTerrain(unsigned short new_group)
{
    this->type = 0;
    this->group = new_group;
    if(new_group == GROUP_WATER)
    {   flags |= FLAG_HAS_UNDERGROUND_WATER;}
}

ConstructionGroup *
MapTile::getTileConstructionGroup() const {
    switch (group)
    {
        case GROUP_BARE:    return &bareConstructionGroup;      break;
        case GROUP_DESERT:  return &desertConstructionGroup;    break;
        case GROUP_WATER:   return &waterConstructionGroup;     break;
        case GROUP_TREE:    return &treeConstructionGroup;      break;
        case GROUP_TREE2:   return &tree2ConstructionGroup;     break;
        case GROUP_TREE3:   return &tree3ConstructionGroup;     break;
        default:
            std::cout << "invalid group of maptile at: (" << point.x <<"," << point.y << ")" << std::endl;
            return &desertConstructionGroup;
    }
}

ResourceGroup* MapTile::getTileResourceGroup() const {
    static bool initialized = false;
    static ResourceGroup* bare = 0;
    static ResourceGroup* desert = 0;
    static ResourceGroup* water = 0;
    static ResourceGroup* tree = 0;
    static ResourceGroup* tree2 = 0;
    static ResourceGroup* tree3 = 0;

    if (!initialized)
    {
        initialized = true;
        //These must match the Group names from all_modules.cpp NOT all_buldings.cpp
        bare = ResourceGroup::resMap["Green"];
        desert = ResourceGroup::resMap["Desert"];
        water = ResourceGroup::resMap["Water"];
        tree = ResourceGroup::resMap["Tree"];
        tree2 = ResourceGroup::resMap["Tree2"];
        tree3 = ResourceGroup::resMap["Tree3"];

    }
    switch (group)
    {
        case GROUP_BARE:    return bare;      break;
        case GROUP_DESERT:  return desert;    break;
        case GROUP_WATER:   return water;     break;
        case GROUP_TREE:    return tree;      break;
        case GROUP_TREE2:   return tree2;     break;
        case GROUP_TREE3:   return tree3;     break;
        default:
            std::cout << "invalid group of maptile at: (" << point.x <<"," << point.y << ")" << std::endl;
            return desert;
    }
}

ConstructionGroup* MapTile::getConstructionGroup() const //constructionGroup of bare land or the covering construction
{   return (reportingConstruction ? reportingConstruction->constructionGroup : getTileConstructionGroup());}

ConstructionGroup* MapTile::getTopConstructionGroup() const //constructionGroup of bare land or the actual construction
{   return (construction ? construction->constructionGroup : getTileConstructionGroup());}

ConstructionGroup* MapTile::getLowerstVisibleConstructionGroup() const
{
    if(!reportingConstruction || reportingConstruction->flags & FLAG_TRANSPARENT)
    {   return getTileConstructionGroup();}
    else
    {   return getConstructionGroup();}
}

unsigned short MapTile::getType() const //type of bare land or the covering construction
{   return (reportingConstruction ? reportingConstruction->frameIt->frame : type);}

unsigned short MapTile::getTopType() const //type of bare land or the actual construction
{   return (construction ? construction->frameIt->frame : type);}

unsigned short MapTile::getLowerstVisibleType() const {
    if(!reportingConstruction || reportingConstruction->flags & FLAG_TRANSPARENT)
    {   return type;}
    else
    {   return reportingConstruction->frameIt->frame;}
}

unsigned short MapTile::getGroup() const //group of bare land or the covering construction
{   return (reportingConstruction ? reportingConstruction->constructionGroup->group : group);}

unsigned short MapTile::getTransportGroup() const //group of bare land or the covering construction
{
    unsigned short grp = getGroup();
    if (is_transport())
    {
        switch(grp)
        {
            case GROUP_TRACK_BRIDGE:
                grp = GROUP_TRACK;
                break;
            case GROUP_ROAD_BRIDGE:
                grp = GROUP_ROAD;
                break;
            case GROUP_RAIL_BRIDGE:
                grp = GROUP_RAIL;
                break;
            default:
                break;
        }
    }
    return grp;
}

unsigned short MapTile::getTopGroup() const //group of bare land or the actual construction
{
    if(!construction) //simple case
    {   return group;}
    else if(construction == reportingConstruction) //regular construction
    {   return construction->constructionGroup->group;}
    else // in case a construction has a chile e.g. waste burning on transport
    {   return (reportingConstruction ? reportingConstruction->constructionGroup->group : group);}
}

unsigned short MapTile::getLowerstVisibleGroup() const {
  if(!reportingConstruction || reportingConstruction->flags & FLAG_TRANSPARENT)
    return group;
  else if(reportingConstruction->constructionGroup->group == GROUP_PORT
    && point.x == reportingConstruction->point.x + 3
  )
    // The water section of a port
    return GROUP_WATER;
  else
    return reportingConstruction->constructionGroup->group;
}


bool MapTile::is_bare() const //true if we there is neither a covering construction nor water
{   return (!reportingConstruction) && (group != GROUP_WATER);}

bool MapTile::is_water() const //true on bridges or lakes (also under bridges)
{   return (group == GROUP_WATER);}

bool MapTile::is_lake() const //true on lakes (also under bridges)
{   return (group == GROUP_WATER) && !(flags & FLAG_IS_RIVER);}

bool MapTile::is_river() const // true on rivers (also under bridges)
{   return (flags & FLAG_IS_RIVER);}

bool MapTile::is_visible() const // true if tile is not covered by another construction. Only useful for minimap Gameview is rotated to upperleft
{   return (construction || !reportingConstruction);}

bool MapTile::is_transport() const //true on tracks, road, rails and bridges
{   return (reportingConstruction && reportingConstruction->flags & FLAG_IS_TRANSPORT);}

bool MapTile::is_residence() const //true on residences
{
    return (reportingConstruction &&(
        (reportingConstruction->constructionGroup->group == GROUP_RESIDENCE_LL)
     || (reportingConstruction->constructionGroup->group == GROUP_RESIDENCE_ML)
     || (reportingConstruction->constructionGroup->group == GROUP_RESIDENCE_HL)
     || (reportingConstruction->constructionGroup->group == GROUP_RESIDENCE_LH)
     || (reportingConstruction->constructionGroup->group == GROUP_RESIDENCE_MH)
     || (reportingConstruction->constructionGroup->group == GROUP_RESIDENCE_HH) ) );
}


std::list<ExtraFrame>& MapTile::ensureFrames()
{
    if(!framesptr)
    {   framesptr = std::make_unique<std::list<ExtraFrame>>();}
    return *framesptr;
}

std::list<ExtraFrame>::iterator MapTile::addFrame(void)
{
    std::list<ExtraFrame>& frames = ensureFrames();
    frames.resize(frames.size() + 1);
    std::list<ExtraFrame>::iterator frit = frames.end();
    std::advance(frit, -1);
#ifndef NDEBUG
    register_frame(&*frit);
#endif
    return frit; //the last position
}

void MapTile::removeFrame(const std::list<ExtraFrame>::iterator& it)
{
    assert(framesptr && "removeFrame: tile owns no frames");
    LNG_LOG(lincity::log::kFrames, lincity::log::kTrace,
      "removeFrame ({},{})", point.x, point.y);
#ifndef NDEBUG
    unregister_frame(&*it); //catches double kills even when the address was
                            //reused by a new node of the same list
    if(!owns_frame(*framesptr, it))
    {
        assert(false && "removeFrame: iterator belongs to another tile's list "
          "(cross-tile kill; would corrupt memory)");
    }
#endif
    framesptr->erase(it);
    if (framesptr->empty())
    {
        framesptr.reset();
    }
}

void MapTile::moveFrameTo(Map& map, MapPoint dest, const std::list<ExtraFrame>::iterator& it)
{
    assert(!(point == dest) && "moveFrameTo: dest must differ from the source tile");
    assert(framesptr && "moveFrameTo: tile owns no frames");
    LNG_LOG(lincity::log::kFrames, lincity::log::kTrace,
      "moveFrameTo ({},{}) -> ({},{})", point.x, point.y, dest.x, dest.y);
#ifndef NDEBUG
    if(!owns_frame(*framesptr, it))
    {
        assert(false && "moveFrameTo: iterator belongs to another tile's list");
    }
#endif
    //splice is O(1) and keeps the moved iterator valid
    MapTile& dst = *map(dest);
    std::list<ExtraFrame>& dstframes = dst.ensureFrames();
    dstframes.splice(dstframes.end(), *framesptr, it);
    if (framesptr->empty())
    {
        framesptr.reset();
    }
}

std::list<ExtraFrame>& MapTile::frames()
{
    assert(framesptr && "frames(): tile owns no frames (guard with hasFrames())");
    return *framesptr;
}

const std::list<ExtraFrame>& MapTile::frames() const
{
    assert(framesptr && "frames(): tile owns no frames (guard with hasFrames())");
    return *framesptr;
}



Map::Map(int map_len) :
  side_len(map_len), recentPoint(map_len / 2, map_len / 2)
{
  maptile.reserve(map_len * map_len);
  for(MapPoint p; p.y < map_len; p.y++)
  for(p.x = 0; p.x < map_len; p.x++) {
    maptile.emplace_back(p);
  }
}

Map::~Map() {
  maptile.clear();
}

#ifndef NDEBUG
/* Debug-only whole-map invariant: every node owned by this map's tiles is
 * a registered live frame, and no node is owned by two tiles. */
void Map::assertFramesConsistent() const {
  std::unordered_set<const ExtraFrame*> seen;
  for(const MapTile& tile : maptile) {
    if(!tile.hasFrames())
      continue;
    for(const ExtraFrame& frame : tile.frames()) {
      assert(g_live_frames.count(&frame) == 1
        && "assertFramesConsistent: frame in a tile list is not registered "
        "(node created without addFrame? heap reuse after double kill?)");
      assert(seen.insert(&frame).second
        && "assertFramesConsistent: same frame node owned by two tiles");
    }
  }
}
#endif

bool Map::is_border(MapPoint point) const {
  return (point.x == 0 || point.y == 0
    || point.x == side_len-1 || point.y == side_len-1);
}

bool Map::is_edge(MapPoint point) const {
  return (point.x == 1 || point.y == 1
    || point.x == side_len-2 || point.y == side_len -2);
}

bool
Map::is_visible(MapPoint point) const {
  return (point.x > 0 && point.y > 0
    && point.x < side_len-1 && point.y < side_len-1);
}

bool Map::maximum(MapPoint p) const {
  int alt = operator()(p)->ground.altitude;
  for(MapPoint q : {p.n(),p.s(),p.w(),p.e(),p.nw(),p.ne(),p.sw(),p.se()})
    if(alt < operator()(q)->ground.altitude) return false;
  return true;
}

bool Map::minimum(MapPoint p) const {
  int alt = operator()(p)->ground.altitude;
  for(MapPoint q : {p.n(),p.s(),p.w(),p.e(),p.nw(),p.ne(),p.sw(),p.se()})
    if(alt > operator()(q)->ground.altitude) return false;
  return true;
}

bool Map::saddlepoint(MapPoint p) const {
  int alt = operator()(p)->ground.altitude;
  int dips = 0;
  bool prevLower = true;
  for(MapPoint q : {p.w(),p.nw(),p.n(),p.ne(),p.e(),p.se(),p.s(),p.sw(),p.w()}){
    bool lower = alt > operator()(q)->ground.altitude;
    if(lower && !prevLower)
      dips++;
    prevLower = lower;
  }
  return dips > 1;
}

bool Map::checkEdgeMin(MapPoint point) const {
  MapPoint q1,q2;
  if(point.x == 1 || point.x == side_len-2) {
    q1 = point.w();
    q2 = point.e();
  }
  else if(point.y == 1 || point.y == side_len-2) {
    q1 = point.n();
    q2 = point.s();
  }
  else
    return false;

  // handle corners
  if(point == MapPoint(1,1))
    q1 = point.s();
  else if(point == MapPoint(1,side_len-2))
    q2 = point.s();
  else if(point == MapPoint(side_len-2,1))
    q1 = point.n();
  else if(point == MapPoint(side_len-2,side_len-2))
    q2 = point.n();

  int alt = operator()(point)->ground.altitude;
  return alt < operator()(q1)->ground.altitude
    && alt < operator()(q2)->ground.altitude;
}

Map::  iterator Map::  begin()       { return maptile.  begin(); }
Map::  iterator Map::  end()         { return maptile.  end();   }
Map:: riterator Map:: rbegin()       { return maptile. rbegin(); }
Map:: riterator Map:: rend()         { return maptile. rend();   }
Map:: citerator Map:: cbegin() const { return maptile. cbegin(); }
Map:: citerator Map:: cend()   const { return maptile. cend();   }
Map::criterator Map::crbegin() const { return maptile.crbegin(); }
Map::criterator Map::crend()   const { return maptile.crend();   }
Map:: citerator Map::  begin() const { return maptile.  begin(); }
Map:: citerator Map::  end()   const { return maptile.  end();   }
Map::criterator Map:: rbegin() const { return maptile. rbegin(); }
Map::criterator Map:: rend()   const { return maptile. rend();   }

World::World() :
  World(WORLD_SIDE_LEN)
{}

World::World(int mapSize) :
  map(mapSize)
{
  total_time = 0;
  coal_survey_done = 0;
  total_money = 0;
  tech_level = 0;
  rockets_launched = 0;
  rockets_launched_success = 0;
  gameEnd = false;

  people_pool = 100;

  stats.sustainability.old_population = people_pool;

  for(Commodity s = STUFF_INIT; s < STUFF_COUNT; s++) {
    tradeRule[s].take = true;
    tradeRule[s].give = true;
  }
}

World::~World() {
    // Constructions are deleted by MapTile::~MapTile, which runs when
    // `map` (declared after `vehicleList`) is destroyed. The
    // commodityLedger is declared before `map` so it outlives those
    // destructors and still debits inventory destruction (TST-03).
    // Vehicles are only owned here: delete the survivors (do_animate
    // already reaped the dead ones).
    for(Vehicle* v : vehicleList)
        delete v;
    vehicleList.clear();
}

void
World::buildConstruction(ConstructionGroup& cstGrp, MapPoint point) {
  Message::ptr message;
  if(!cstGrp.can_build(*this, message))
    CannotBuildMessage::create(cstGrp, message)->throwEx();
  if(!cstGrp.can_build_here(*this, point, message))
    CannotBuildHereMessage::create(cstGrp, point, message)->throwEx();
  // Charge the group that will actually be built: transport over water
  // becomes a bridge (Transport::place swaps the group), which costs
  // BRIDGE_FACTOR x the road (BUG-04).
  ConstructionGroup& chargedGroup = getConfig()->bridgeRealCost.get()
    ? transport_group_at(cstGrp, *map(point)) : cstGrp;
  expense(chargedGroup.getCosts(*this), stats.expenses.construction,
    !chargedGroup.no_credit);
  cstGrp.placeItem(*this, point);

  map.connect_transport(point.x - 2, point.y - 2,
    point.x + cstGrp.size + 1, point.y + cstGrp.size + 1);
  map.desert_water_frontiers(point.x - 1, point.y - 1,
    cstGrp.size + 2, cstGrp.size + 2);
}

void
World::bulldozeArea(MapPoint point) {
  if(!map.is_visible(point))
    OutsideMapMessage::create(point)->throwEx();

  if(Construction *cst = map(point)->reportingConstruction) {
    cst->bulldoze();
  }
  else {
    MapTile& tile = *map(point);
    unsigned short g = tile.getGroup();
    if(g == GROUP_DESERT)
      return; // nothing to do

    expense(tile.getTileConstructionGroup()->bul_cost,
      stats.expenses.construction);

    if(g == GROUP_WATER) {
      tile.group = GROUP_BARE;
      tile.flags &= ~(FLAG_IS_RIVER);
      tile.flags &= ~(FLAG_POWER_CABLES_0 | FLAG_POWER_CABLES_90);
      map.connect_rivers(point.x, point.y);
    }
    else {
      tile.group = GROUP_DESERT;
    }
    map.desert_water_frontiers(point, point.se());
  }

  setUpdated(Updatable::MAP);
}

void
World::evacuateArea(MapPoint point) {
  if(!map.is_visible(point))
    OutsideMapMessage::create(point)->throwEx();
  Construction *cst = map(point)->reportingConstruction;
  if(!cst)
    NothingHereMessage::create(point)->throwEx();
  if(cst->flags & FLAG_NEVER_EVACUATE)
    CannotEvacuateThisMessage::create(point,
      *cst->constructionGroup)->throwEx();

  if(cst->constructionGroup->group == GROUP_MARKET) {
    dynamic_cast<Market*>(cst)->toggleEvacuation();
    return;
  }
  cst->flags ^= FLAG_EVACUATE;
}

void
World::floodArea(MapPoint point) {
  if(!map.is_visible(point))
    OutsideMapMessage::create(point)->throwEx();
  if(!map(point)->is_bare())
    SpaceOccupiedMessage::create(point)->throwEx();
  map(point)->setTerrain(GROUP_WATER);
  expense(GROUP_WATER_COST, stats.expenses.construction);
  map.connect_transport(point.x - 2, point.y - 2, point.x + 2, point.y + 2);
  map.desert_water_frontiers(point.x - 1, point.y - 1, 3, 3);
  map.connect_rivers(point.x, point.y);
  setUpdated(Updatable::MAP);
}

void
World::pushMessage(Message::ptr message) {
  messageQueue.push_back(message);
}

Message::ptr
World::popMessage() {
  if(messageQueue.empty())
    return Message::ptr();
  Message::ptr message = messageQueue.front();
  messageQueue.pop_front();
  return message;
}

void
World::setUpdated(Updatable what) {
  updatedSet.insert(what);
}

void
World::clearUpdated(Updatable what) {
  updatedSet.erase(what);
}

bool
World::isUpdated(Updatable what) {
  return updatedSet.find(what) != updatedSet.end();
}

bool
World::hasBuildingNeighbor(MapPoint point) const {
  for(MapPoint nb : {point.n(), point.s(), point.e(), point.w()}) {
    if(!map.is_inside(nb)) continue;
    const Construction *cst = map(nb)->reportingConstruction;
    if(cst
      && !(cst->flags & FLAG_IS_TRANSPORT)
      && !(cst->flags & FLAG_TRANSPARENT)
    ) {
      return true;
    }
  }
  return false;
}

/** @file lincity/world.cpp */
