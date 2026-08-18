/* ---------------------------------------------------------------------- *
 * src/lincity/world.hpp
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

#ifndef __world_h__
#define __world_h__

#include <array>            // for array
#include <cassert>          // for assert
#include <deque>            // for deque
#include <filesystem>       // for path
#include <iostream>         // for ostream
#include <list>             // for list
#include <memory>           // for unique_ptr
#include <optional>
#include <set>              // for set
#include <string>           // for basic_string, string
#include <unordered_set>    // for unordered_set
#include <vector>           // for vector

#include "MapPoint.hpp"     // for MapPoint
#include "all_buildings.hpp"  // for COAL_TAX_RATE, DOLE_RATE, GOODS_TAX_RATE
#include "commodities.hpp"  // for Commodity, CommodityRule (ptr only)
#include "messages.hpp"     // for Message
#include "resources.hpp"    // for ExtraFrame, ResourceGroup (ptr only)
#include "stats.hpp"          // for Stat, Stats

class Construction;
class ConstructionGroup;
class Vehicle;

#define WORLD_SIDE_LEN 100

class Ground {
public:
  Ground();
  ~Ground();
  int altitude;       //surface of ground. unused currently
  int ecotable;       //done at init time: pointer to the table for vegetation
  int wastes;         //wastes underground
  int pollution;      //pollution underground
  int water_alt;      //altitude of water (needed to know drainage basin)
  int water_pol;      //pollution of water
  int water_wast;     //wastes in water
  int water_next;     //next tile(s) where the water will go from here
  int int1;           //reserved for future (?) use
  int int2;
  int int3;
  int int4;
};

class Map; // forward declaration — MapTile::moveFrameTo needs the Map owning it

/* BUG-12 (R9): orders Map::constructions by the fixed main-tile point
 * (unique, never mutated after registration) instead of by heap address,
 * making iteration — and thus the whole sim trajectory — independent of
 * memory layout. operator() is out-of-line in world.cpp. */
struct ConstructionPointLess {
  bool operator()(const Construction* a, const Construction* b) const;
};

class MapTile {
public:
  MapTile(MapPoint point);
  ~MapTile();
  /* Movable (std::vector<MapTile> requires it) but not copyable: it owns
   * raw/unique pointers that would be freed twice. Map reserves the exact
   * vector capacity, so tiles never actually move at runtime. */
  MapTile(MapTile&& other) noexcept;

  // TODO: make point const. Attention to map_len assignment in load/save.
  //       Option 1: refactor load/save so World::map needn't be assigned.
  //       Option 2: mess with the allocator for the Map::maptile vector.
  //       see https://en.cppreference.com/w/cpp/container/vector/operator=
  //       Option 3: convert Map::maptile to a vector<unique_ptr<MapTile>>
  MapPoint point;
  Ground ground;                        //the Ground associated to an instance of MapTile
  Construction *construction;           //the actual construction (e.g. for simulation)
  Construction *reportingConstruction;  //the construction covering the tile
  unsigned short type;                  //type of terrain (underneath constructions)
  unsigned short group;                 //group of the terrain (underneath constructions)
  unsigned int flags;                   //flags are defined in lin-city.h
  // TODO: prevent access to coal_reserve when coal_survey_done == false
  unsigned short coal_reserve;          //underground coal
  unsigned short ore_reserve;           //underground ore
  int pollution;                        //air pollution (under ground pollution is in ground[][])

  void setTerrain(unsigned short group); //places type & group at MapTile

  /* ExtraFrame ownership (R8): fully encapsulated. The list is allocated
   * on demand and deleted when it empties; callers only hold iterators,
   * which stay valid across removeFrame of other elements and across
   * moveFrameTo until their own removal. */
  bool hasFrames() const noexcept { return framesptr != nullptr; } //false iff the tile owns no overlay
  std::list<ExtraFrame>::iterator addFrame(); //appends a default ExtraFrame, returns iterator to it
  void removeFrame(const std::list<ExtraFrame>::iterator& it); //kills an extraframe; asserts ownership in debug builds
  // Moves a frame to another tile without invalidating the iterator
  // (splice is O(1) and stable); allocates the destination list on
  // demand, deletes the source list if it empties.
  void moveFrameTo(Map& map, MapPoint dest, const std::list<ExtraFrame>::iterator& it);
  std::list<ExtraFrame>& frames();            //precondition: hasFrames(); for iteration (draw, vehicle spawn checks)
  const std::list<ExtraFrame>& frames() const;

  unsigned short getType() const;          //type of bare land or the covering construction
  unsigned short getTopType() const;       //type of bare land or the actual construction
  unsigned short getLowerstVisibleType() const ; //like getType but type of terrain underneath transparent constructions
  unsigned short getGroup() const;        //group of bare land or the covering construction
  unsigned short getTopGroup() const;     //group of bare land or the actual construction
  unsigned short getLowerstVisibleGroup() const; //like getGroup but group of terrain underneath transparent constructions
  unsigned short getTransportGroup() const; //like getGroup but bridges are reported normal transport tiles
  ConstructionGroup* getTileConstructionGroup() const; //constructionGroup of the maptile
  ResourceGroup*     getTileResourceGroup() const;     //resourceGroup of a tile
  ConstructionGroup* getConstructionGroup() const;     //constructionGroup of maptile or the covering construction
  ConstructionGroup* getTopConstructionGroup() const;  //constructionGroup of maptile or the actual construction
  ConstructionGroup* getLowerstVisibleConstructionGroup() const;

  bool is_bare() const;                    //true if we there is neither a covering construction nor water
  bool is_lake() const;                    //true on lakes (also under bridges)
  bool is_river() const;                   //true on rivers (also under bridges)
  bool is_water() const;                   //true on bridges or lakes (also under bridges)
  bool is_visible() const;                 //true if tile is not covered by another construction. Only useful for minimap Gameview is rotated to upperleft
  bool is_transport() const;               //true on tracks, road, rails and bridges
  bool is_residence() const;               //true if any residence covers the tile
  void writeTemplate();              //create maptile template
  void saveMembers(std::ostream *os);//write maptile AND ground members as XML to stram

private:
  std::list<ExtraFrame>& ensureFrames(); //allocates the frame list on demand; single allocation point

  //manipulate via addFrame/removeFrame/moveFrameTo
  std::unique_ptr<std::list<ExtraFrame>> framesptr;
};

class Map {
public:
  Map(int map_len);
  ~Map();
  /* Move-only (MapTile is not copyable); declared explicitly because the
   * user-declared destructor suppresses the implicit move ops, and the
   * load path assigns a fresh map (`world.map = Map(len)`). */
  Map(Map&&) = default;
  Map& operator=(Map&&) = default;
  const MapTile* operator()(MapPoint point) const {
    assert(is_inside(point));
    return &(maptile[point.x + point.y * side_len]);
  }
  MapTile* operator()(MapPoint point) {
    assert(is_inside(point));
    return &(maptile[point.x + point.y * side_len]);
  }
  bool is_inside(MapPoint loc) const {
    return loc.x >= 0 && loc.y >= 0
      && loc.x < side_len && loc.y < side_len;
  }
  bool is_border(MapPoint loc) const;
  bool is_edge(MapPoint point) const;
  bool is_visible(MapPoint loc) const;
  int len() const { return side_len; } //tells the actual world.side_len
#ifndef NDEBUG
  /* Debug-only whole-map invariant, checked after every do_time_step. */
  void assertFramesConsistent() const;
#endif
  bool maximum(MapPoint point) const;
  bool minimum(MapPoint point) const;
  bool saddlepoint(MapPoint point) const;
  bool checkEdgeMin(MapPoint point) const;
  std::vector<MapPoint> polluted;
  using iterator = std::vector<MapTile>::iterator;
  using riterator = std::vector<MapTile>::reverse_iterator;
  using citerator = std::vector<MapTile>::const_iterator;
  using criterator = std::vector<MapTile>::const_reverse_iterator;
  iterator begin();
  iterator end();
  riterator rbegin();
  riterator rend();
  citerator cbegin() const;
  citerator cend() const;
  criterator crbegin() const;
  criterator crend() const;
  citerator begin() const;
  citerator end() const;
  criterator rbegin() const;
  criterator rend() const;

  int alt_min, alt_max, alt_step;

  // Using std::set instead of std::unordered_set so iterators remain valid
  // after insertion. Ordered by main-tile point (BUG-12): deterministic
  // iteration independent of heap layout; see ConstructionPointLess.
  std::set<Construction *, ConstructionPointLess> constructions;

  MapPoint recentPoint;


  std::optional<MapPoint> find_group(MapPoint p, unsigned short group);
  std::optional<MapPoint> find_bare_area(MapPoint p, int size);
  void connect_transport(int originx, int originy, int lastx, int lasty);
  void desert_water_frontiers(MapPoint p0, MapPoint p1);
  void desert_water_frontiers(int originx, int originy, int w, int h);
  void connect_rivers(int x, int y);

protected:
  int side_len;
  std::vector<MapTile> maptile;

private:

  bool is_bare_area(MapPoint point, int size);
};

class World {
public:
  World();
  World(int mapSize);
  ~World();

  void save(const std::filesystem::path& filename) const;
  static std::unique_ptr<World> load(const std::filesystem::path& filename);

  void do_time_step();
  void do_animate(unsigned long real_time);
  void buildConstruction(ConstructionGroup& cstGrp, MapPoint point);
  void bulldozeArea(MapPoint point);
  void evacuateArea(MapPoint point);
  void floodArea(MapPoint point);

  void pushMessage(Message::ptr message);
  Message::ptr popMessage();

  /* true if one of the orthogonal neighbours of point is a real building
   * (not transport, not transparent vegetation). Used to spawn cars near
   * buildings and to let them die when they reach one. */
  bool hasBuildingNeighbor(MapPoint point) const;

  enum class Updatable {
    POPULATION,
    TECH,
    MONEY,
    FOOD,
    LABOR,
    GOODS,
    COAL,
    ORE,
    STEEL,
    POLLUTION,
    LOVOLT,
    HIVOLT,
    WATER,
    WASTE,
    TIME,
    MAP,
    SUSTAINABILITY,
  };
  void setUpdated(Updatable what);
  void clearUpdated(Updatable what);
  bool isUpdated(Updatable what);

// private: // planning to remove from public API

  /* TST-03: commodity conservation ledger. Every change to a construction
   * inventory is recorded here (produceStuff/consumeStuff/levelStuff and
   * the destructor). The sim test checks that the sum of all construction
   * inventories equals the baseline plus this ledger, so any code path
   * that creates or destroys commodities without going through those
   * chokepoints breaks the invariant. Declared before `map` so it
   * outlives the map: constructions are deleted from MapTile's destructor
   * during World teardown and still debit the ledger. */
  std::array<long long, STUFF_COUNT> commodityLedger = {};

  Map map;

  std::string given_scene;

  int total_time;  // game time

  int people_pool;

  int total_money;

  struct {
    int income_tax = INCOME_TAX_RATE;
    int coal_tax = COAL_TAX_RATE;
    int ore_tax = ORE_TAX_RATE;
    int dole = DOLE_RATE; // unemployment
    int transport_cost = TRANSPORT_COST_RATE;
    int goods_tax = GOODS_TAX_RATE;
    int export_tax = 0; // unused for now
    int import_cost = IM_PORT_COST_RATE;
  } money_rates;

  struct {
    int labor = 0;
    int coal = 0;
    int ore = 0;
    int goods = 0;
    int trade_ex = 0;
    // int trade_im; // import is payed for directly
  } taxable;

  int tech_level;


  int rockets_launched, rockets_launched_success;
  int coal_survey_done;
  bool gameEnd;

  std::array<CommodityRule, STUFF_COUNT> tradeRule;

  Stats stats;

  std::list<Vehicle*> vehicleList;



  void income(int amt, Stat<int>& account);
  void expense(int amt, Stat<int>& account, bool allowCredit = true);
  void place_item(ConstructionGroup& cstGrp, MapPoint loc);
  void do_pollution();
  void scan_pollution();
  void do_fire_health_cricket_power_cover();
  void do_random_fire();
  void do_daily_ecology();
  void do_coal_survey();

private:
  std::deque<Message::ptr> messageQueue;
  std::unordered_set<Updatable> updatedSet;
  // scratch buffer reused by simulate_mappoints to avoid re-allocation
  std::vector<std::set<Construction*>::iterator> orderingBuffer;

  void do_periodic_events();
  void end_of_month_update();
  void start_of_year_update();
  void end_of_year_update();
  void simulate_mappoints();
  void sustainability_test();
  bool sust_fire_cover();
};



#endif /* __world_h__ */

/** @file lincity/world.h */
