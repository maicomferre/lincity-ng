/* ---------------------------------------------------------------------- *
 * src/lincity/Vehicles.cpp
 * This file is part of Lincity-NG.
 *
 * Copyright (C) 1995-1997 I J Peters
 * Copyright (C) 1997-2005 Greg Sharp
 * Copyright (C) 2000-2004 Corey Keasling
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

#include "Vehicles.hpp"

#include <stdlib.h>         // for NULL
#include <bitset>           // for bitset
#include <cassert>          // for assert
#include <cmath>            // for ceil
#include <map>              // for map
#include <random>           // for discrete_distribution
#include <string>           // for basic_string, operator<
#include <vector>           // for vector

#include "commodities.hpp"  // for Commodity
#include "groups.hpp"       // for GROUP_ROAD_BRIDGE, GROUP_TRACK_BRIDGE
#include "lintypes.hpp"     // for Construction
#include "resources.hpp"    // for ExtraFrame, ResourceGroup
#include "util/randutil.hpp"         // for BasicUrbg
#include "world.hpp"        // for World, Map, MapTile

Vehicle::Vehicle(World& world, MapPoint point, VehicleModel model0,
  VehicleStrategy vehicleStrategy
) :
  world(world), point(point)
{
  this->origin = point;
  this->next = this->prev = this->old1 = this->old2 = point;
  this->anim = 0;
  this->death_counter = 100;
  this->wait_ticks = 10;
  this->arriving = false;
  this->arrival_start = 0;
  this->arrival_side = 0;
  this->turn_left = true;
  this->headings = 0;
  this->direction = 0;
  this->xr = (double)point.x;
  this->yr = (double)point.y;

  this->stuff_id = STUFF_LABOR;
  //TODO Choose a random model for suitable stuff
  this->model = model0;
  this->strategy = vehicleStrategy;
  this->initial_cargo = world.map(point)->reportingConstruction
    ? world.map(point)->reportingConstruction->tellstuff(stuff_id,-2)
    : -1;
  pickDestination();

  frameIt = world.map(point)->createframe();
  frameIt->frame = -2; //special value to indicate fresh fast forward car
  framePt = point;
  switch(model) {
  case VEHICLE_BLUECAR:
    frameIt->resourceGroup =  ResourceGroup::resMap["Bluecar"];
    speed0 = BLUE_CAR_SPEED;
    break;
  default:
    assert(false);
  }
  speed = speed0;
}

Vehicle::~Vehicle() {
  world.vehicleList.remove(this);
  world.map(framePt)->killframe(frameIt);
}

void Vehicle::drive(void) {
  --death_counter;
  speed = speed0;
  int xstep = next.x - prev.x;
  int ystep = next.y - prev.y;
  if(xstep == 2)
    direction = 2;
  else if(xstep == -2)
    direction = 6;
  else if(ystep == 2)
    direction = 0;
  else if(ystep == -2)
    direction = 4;
  else if(xstep == 1 && ystep == 1) {
    direction = 1;            //vertically down
    turn_left = prev.x == point.x; //turning left
  }
  else if(xstep == 1 && ystep == -1) {
    direction = 3; //horizontally right
    turn_left = prev.x != point.x; //turning left and YES == is correct
  }
  else if( (xstep == -1) && (ystep == 1) ) {
    direction = 7;
    turn_left = prev.x != point.x; //turning left and YES == is correct
  }
  else if(xstep == -1 && ystep == -1) {
    direction = 5;            //vertically up
    turn_left = prev.x == point.x; //turning left
  }

  if(direction&1)
    speed = (turn_left ? 3 : 1) * speed0 / 2;

  //now advance to position of the vehicle
  old2 = old1;
  old1 = prev;
  prev = point;
  point = next;

  if(frameIt->frame < -1)
    ++frameIt->frame;
  else {
    int s = (int)frameIt->resourceGroup->graphicsInfoVector.size();
    if(s)
      frameIt->frame = direction % s;
  }
}

void Vehicle::walk(unsigned long real_time) {
  double remaining = (double)(anim - real_time) / speed;
  double elapsed = 1 - remaining;
  //mx,my are like continuos coordinates for the map
  //(0,0) is always the southern corner of the tiles
  double mx = 0;
  double my = 0;
  switch(direction) { //pointing towards
  case 0: //lower left
    mx = -0.65;
    my = -remaining;
  break;
  case 4: //upper right
    mx = -0.27;
    my = -elapsed;
  break;
  case 2: //lower right
    mx = -remaining;
    my = -0.27;
  break;
  case 6: //upper left
    mx = -elapsed;
    my = -0.65;
  break;
  case 1: { //vertically down
    if(turn_left) {
      mx = -0.65 + elapsed * elapsed;
      my = -1.13 + 1 * elapsed;
    }
    else {
      mx =  -1 + 0.35 * elapsed;
      my = -0.27 + 0.27 * elapsed * elapsed;
    }
    break;
  }
  case 5: { //vertically up
    if(turn_left) {
      mx = -0.27 - 0.73 * elapsed * elapsed;
      my =  -0.65 * elapsed;
    }
    else {
      mx = 0 - 0.23 * elapsed;
      my = -0.65 - 0.35 * elapsed * elapsed;
    }
    break;
  }
  case 3: { //horizontally right
    if(turn_left) {
      if(elapsed < 0.5) {
        mx = -1 + elapsed;
        my =  -0.27 - 0.5 * elapsed * elapsed;
      }
      else {
        mx = -0.27 - 0.5 * remaining * remaining;
        my = -1 + remaining;
      }
    }
    else {
      mx = -0.27 * remaining * remaining;
      my = -0.27 * elapsed;
    }
    break;
  }
  case 7: { //horizontally left
    if(turn_left) {
      mx = -0.65 * elapsed;
      my = -0.65 + 0.65 * elapsed * elapsed;
    }
    else {
      mx = -0.65 - 0.35 * elapsed * elapsed;
      my = -1 + 0.35 * elapsed;
    }
    break;
  }
  }

  // a car with (mx,my)==(0,0) has its center in the southern corner
  mx += 0.25;
  my += 0.25;
  //update absolute floating positions
  xr = (double)prev.x + mx;
  yr = (double)prev.y + my;
  //choose tile for placing the frame
  MapPoint tile(ceil(xr), ceil(yr));
  //no need to go up or left
  if(tile.x < prev.x) tile.x = prev.x;
  if(tile.y < prev.y) tile.y = prev.y;
  //align animation to placement of frame on map
  mx = xr - (double)tile.x;
  my = yr - (double)tile.y;
  if(tile != framePt)
    move_frame(tile);

  //Apply the animation of the sprite
  //dx, dy are on-screen coordinates
  int dx = (mx - my) * 64;
  int dy = (mx + my) * 32;

  // Arriving at a building: drift the sprite sideways towards the building
  // as if pulling into a driveway. The car holds its tile but slides
  // off-center over ARRIVE_MS before disappearing.
  if(arriving) {
    double prog = (double)(real_time - arrival_start) / ARRIVE_MS;
    if(prog > 1.0) prog = 1.0;
    double slide = prog * prog * 14; // ease-out, max ~14px
    int side = arrival_side; // 0 = north/up side, 1 = south/down side, etc.
    switch(side) {
    case 0: dy -= (int)slide; dx += (int)(slide / 2); break; // up (north)
    case 1: dy += (int)slide; dx -= (int)(slide / 2); break; // down (south)
    case 2: dx += (int)slide; break;                          // right (east)
    case 3: dx -= (int)slide; break;                          // left (west)
    default: break;
    }
  }

  //Check for bridges and ramps
  switch(world.map(prev)->getGroup()) {
  case GROUP_TRACK_BRIDGE:
    dy -= TRACK_BRIDGE_HEIGHT;
    break;
  case GROUP_ROAD_BRIDGE:
    dy -= ROAD_BRIDGE_HEIGHT;
    break;
  case GROUP_ROAD:
    if(world.map(next)->getGroup() == GROUP_ROAD_BRIDGE
      || world.map(point)->getGroup() == GROUP_ROAD_BRIDGE
    ) {
      dy -= elapsed * ROAD_BRIDGE_HEIGHT / 2;
      frameIt->frame = 8 + direction/2; //going up hill
      if(world.map(point)->getGroup() == GROUP_ROAD_BRIDGE)
        dy -= ROAD_BRIDGE_HEIGHT / 2;
    }
    else if(world.map(old2)->getGroup() == GROUP_ROAD_BRIDGE
      || world.map(old1)->getGroup() == GROUP_ROAD_BRIDGE
    ) {
      dy -= remaining * ROAD_BRIDGE_HEIGHT / 2;
      frameIt->frame = 12 + direction/2; //going down hill
      if(world.map(old1)->getGroup() == GROUP_ROAD_BRIDGE)
        dy -= ROAD_BRIDGE_HEIGHT / 2;
    }
    break;
  case GROUP_TRACK:
    if(world.map(point)->getGroup() == GROUP_TRACK_BRIDGE) {
      dy -= elapsed * TRACK_BRIDGE_HEIGHT;
      frameIt->frame = 8 + direction/2; //going up hill
    }
    else if(world.map(old1)->getGroup() == GROUP_TRACK_BRIDGE) {
      dy -= remaining * TRACK_BRIDGE_HEIGHT;
      frameIt->frame = 12 + direction/2;// going down hill
    }
    break;
  }

  frameIt->move_x = dx;
  frameIt->move_y = dy;
}


void
Vehicle::move_frame(MapPoint newPoint) {
  if(!world.map(newPoint)->framesptr)
    world.map(newPoint)->framesptr = new std::list<ExtraFrame>;
  world.map(newPoint)->framesptr->splice(
    world.map(newPoint)->framesptr->end(),
    *world.map(framePt)->framesptr,
    frameIt
  );
  if(world.map(framePt)->framesptr->empty()) {
    delete world.map(framePt)->framesptr;
    world.map(framePt)->framesptr = NULL;
  }
  framePt = newPoint; //remember where the frame was put
}

bool Vehicle::tileOccupied(MapPoint p) const {
  std::list<ExtraFrame> *frames = world.map(p)->framesptr;
  if(!frames) return false;
  for(const ExtraFrame& exfr : *frames)
    if(exfr.resourceGroup && exfr.resourceGroup->is_vehicle)
      return true;
  return false;
}

int Vehicle::buildingDirection(MapPoint p) const {
  struct { MapPoint nb; int dir; } sides[4] = {
    {p.n(), 0}, {p.s(), 1}, {p.e(), 2}, {p.w(), 3}
  };
  for(auto& s : sides) {
    if(!world.map.is_inside(s.nb)) continue;
    const Construction *cst = world.map(s.nb)->reportingConstruction;
    if(cst
      && !(cst->flags & FLAG_IS_TRANSPORT)
      && !(cst->flags & FLAG_TRANSPARENT)
    ) {
      return s.dir;
    }
  }
  return -1;
}

bool Vehicle::acceptable_heading(MapPoint dest) {
  unsigned short g = world.map(dest)->getTransportGroup();

  if(g != GROUP_TRACK && g != GROUP_ROAD) {
    if(g != GROUP_RAIL) return false;
    if(!world.map.is_visible(dest)) return false;
    unsigned short g2 = world.map(prev)->getTransportGroup();
    unsigned short g3 = world.map(MapPoint(2*dest.x-point.x, 2*dest.y-point.y))
      ->getTransportGroup();
    if(g2 != g3) return false;
  }

  //handle trivial case
  if(strategy == VEHICLE_STRATEGY_RANDOM) return true;

  //dont go nowhere
  if(!world.map(dest)->reportingConstruction) return false;
  //always leave from illegal area
  if(!world.map(point)->reportingConstruction) return true;

  switch(strategy) {
  case VEHICLE_STRATEGY_MAXIMIZE:
    return world.map(point)->reportingConstruction->tellstuff(stuff_id,-2)*24/25
        < world.map(dest)->reportingConstruction->tellstuff(stuff_id,-2)
      && initial_cargo*99/100
        < world.map(dest)->reportingConstruction->tellstuff(stuff_id,-2);
  case VEHICLE_STRATEGY_MINIMIZE:
    return world.map(point)->reportingConstruction->tellstuff(stuff_id,-2)
        > world.map(dest)->reportingConstruction->tellstuff(stuff_id,-2)*24/25
      && initial_cargo
        > world.map(dest)->reportingConstruction->tellstuff(stuff_id,-2)*99/100;
  default: //silence warning
    return true;
  }
  return false;
}


void Vehicle::pickDestination() {
  // Choose a road tile that has a real building next to it as the trip's
  // end point, so the car visibly "arrives" at a building's front door.
  // Search at a distance comparable to MIN_VEHICLE_TRIP so the car actually
  // drives far enough before reaching it (otherwise it would pass through
  // the destination before being allowed to stop).
  destination = point;
  for(int r = MIN_VEHICLE_TRIP; r <= 40; ++r) {
    for(int dx = -r; dx <= r; ++dx) {
      for(int dy = -r; dy <= r; ++dy) {
        if(std::abs(dx) != r && std::abs(dy) != r) continue; // ring only
        MapPoint cand(point.x + dx, point.y + dy);
        if(!world.map.is_inside(cand)) continue;
        if(cand == origin) continue;
        if(world.map(cand)->getTransportGroup() != GROUP_ROAD) continue;
        if(!world.hasBuildingNeighbor(cand)) continue; // a front door
        destination = cand;
        return;
      }
    }
  }
}


void Vehicle::getNewHeadings() {
  std::bitset<4> headings;

  //never turn back the car, and don't drive onto a tile another vehicle
  //already occupies (1 car per tile, prevents overlapping at intersections)
  if(point.x >= prev.x && acceptable_heading(point.e()) && !tileOccupied(point.e()))
    headings.set(0);
  if(point.x <= prev.x && acceptable_heading(point.w()) && !tileOccupied(point.w()))
    headings.set(1);
  if(point.y <= prev.y && acceptable_heading(point.n()) && !tileOccupied(point.n()))
    headings.set(2);
  if(point.y >= prev.y && acceptable_heading(point.s()) && !tileOccupied(point.s()))
    headings.set(3);

  // If there is no way to go forward (we drove into a dead end), allow
  // turning around: the only remaining road is where we came from. Only die
  // if there is truly no road at all.
  if(headings.none()) {
    bool hasExit = false;
    for(MapPoint nb : {point.e(), point.w(), point.n(), point.s()}) {
      if(!world.map.is_inside(nb)) continue;
      unsigned short g = world.map(nb)->getTransportGroup();
      if(g == GROUP_TRACK || g == GROUP_ROAD || g == GROUP_RAIL) {
        hasExit = true;
        break;
      }
    }
    if(!hasExit) {
      death_counter = 0; // true dead end, nowhere at all
      return;
    }
    // dead end on the road network: turn around and go back the way we came
    headings.reset();
    if(point.x < prev.x && !tileOccupied(point.e()))
      headings.set(0);
    if(point.x > prev.x && !tileOccupied(point.w()))
      headings.set(1);
    if(point.y < prev.y && !tileOccupied(point.s()))
      headings.set(2);
    if(point.y > prev.y && !tileOccupied(point.n()))
      headings.set(3);
    if(headings.none()) {
      // also blocked on the way back (e.g. another car) -> wait, don't die
      return;
    }
  }

  // bias the choice towards moving closer to the trip destination (Manhattan
  // distance). Branches that reduce the distance get extra weight, so cars
  // visibly drive towards a building instead of wandering randomly. If every
  // towards-destination exit is occupied, the other free exits keep a base
  // weight so the car can still move (avoids mutual blocking on 2-lane roads).
  MapPoint branches[4] = {point.e(), point.w(), point.n(), point.s()};
  float weights[4] = {0, 0, 0, 0};
  int dx = destination.x - point.x;
  int dy = destination.y - point.y;
  for(int i = 0; i < 4; ++i) {
    if(!headings[i]) continue;
    weights[i] = 1.f;
    if(destination != point) {
      int ndx = destination.x - branches[i].x;
      int ndy = destination.y - branches[i].y;
      if(std::abs(ndx) + std::abs(ndy) < std::abs(dx) + std::abs(dy))
        weights[i] += 8.f; // towards the goal
    }
  }

  int choice = std::discrete_distribution({weights[0], weights[1],
    weights[2], weights[3]})(BasicUrbg::get());

  //set the next destination
  switch(choice) {
  case 0: next = point.e(); break;
  case 1: next = point.w(); break;
  case 2: next = point.n(); break;
  case 3: next = point.s(); break;
  }
}


void
Vehicle::update(unsigned long real_time) {
  // playing the arriving animation: hold still, drift sideways towards the
  // building, then disappear. No new heading or driving during this.
  if(arriving) {
    walk(real_time);
    if(real_time >= arrival_start + ARRIVE_MS)
      death_counter = 0;
    if(death_counter <= 0)
      delete this;
    return;
  }

  //get a new heading
  if(point == next)
    getNewHeadings();
  // If we are blocked (no heading was chosen because another car occupies
  // every exit), point == next and we simply wait: retry the heading on the
  // next update without driving. A blocked car fades out quickly
  // (wait_ticks) so vehicles never look permanently stacked.
  if(point == next) {
    if(death_counter <= 0) {
      delete this;
      return;
    }
    if(real_time > anim) {
      anim = real_time + speed;
      if(--wait_ticks <= 0)
        death_counter = 0;
    }
    return;
  }
  if(real_time > anim) { //move to dest
    drive();
    // let the car start its arrival animation when it reaches its destination
    // road tile (as if it had arrived at a building's front door). It must
    // first leave its spawn building behind, so only consider reaching the
    // destination after a minimum travel distance (death_counter counts down
    // from 100 per tile driven). Cars that never reach a building still die
    // from the lifespan fallback.
    if(destination != point
      && death_counter <= 100 - MIN_VEHICLE_TRIP
      && std::abs(destination.x - point.x) + std::abs(destination.y - point.y) <= 1
    ) {
      arriving = true;
      arrival_start = real_time;
      // drift towards the side of the road the destination building is on
      arrival_side = buildingDirection(destination);
      return; // wait for the arrival animation to finish
    }
    if (frameIt->frame < 0)
      anim = real_time + 50;
    else
      anim = real_time + speed;
  }
  //animate the sprite
  walk(real_time);
  //cars have limited lifespan
  if(death_counter <= 0)
    delete this;
}
