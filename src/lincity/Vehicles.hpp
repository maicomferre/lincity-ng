/* ---------------------------------------------------------------------- *
 * src/lincity/Vehicles.hpp
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

#ifndef __Vehicles_h__
#define __Vehicles_h__

#include <list>  // for list

#include "MapPoint.hpp"

class World;
enum Commodity : int;
struct ExtraFrame;

#define BLUE_CAR_SPEED 1500
#define TRACK_BRIDGE_HEIGHT 22
#define ROAD_BRIDGE_HEIGHT 44

#define COMMUTER_TRAFFIC_RATE 1024

/* minimum number of tiles a vehicle must drive before it may disappear by
 * reaching its destination building (so it visibly leaves its spawn building
 * and covers a few blocks rather than vanishing after a single one) */
#define MIN_VEHICLE_TRIP 40

enum VehicleModel {
  VEHICLE_BLUECAR,
  VEHICLE_REDCAR,
  VEHICLE_GREENCAR,
  VEHICLE_YELLOWCAR,
  VEHICLE_WHITECAR,
  VEHICLE_FIRETRUCK,
  VEHICLE_DEFAULT
};

enum VehicleStrategy {
  VEHICLE_STRATEGY_MAXIMIZE, //go towards more stuff eg. morning commute for STUFF_LABOR
  VEHICLE_STRATEGY_MINIMIZE, //go towards less stuff eg. evening commute for STUFF_LABOR
  VEHICLE_STRATEGY_RANDOM    //just do a random walk
};

class Vehicle {
public:
  Vehicle(World& world, MapPoint point, VehicleModel model0,
    VehicleStrategy vehicleStrategy = VEHICLE_STRATEGY_RANDOM);

  ~Vehicle(void);

  World& world;
  //location, heading and comming from
  MapPoint point, next, prev, old1, old2;
  // where the car spawned; used to only die when reaching another building
  MapPoint origin;
  // building the car is driving towards (its trip destination)
  MapPoint destination;
  float xr, yr;
  int death_counter;
  /* REF-03e (rule R8): set instead of `delete this` when the vehicle's
   * time is up; World::do_animate() reaps dead vehicles after the update
   * pass, so no object destroys itself while its list is iterated. */
  bool dead = false;
  // ticks left before a blocked car gives up and disappears (anti-deadlock)
  int wait_ticks;
  // true while the car plays its "arriving" animation at the destination
  bool arriving;
  unsigned long arrival_start;
  // lateral direction the car turns while arriving (0=left,1=right)
  int arrival_side;
  bool turn_left;
  unsigned int headings;
  int direction;

  VehicleModel model; //different vehicles
  Commodity stuff_id; //cargo
  int initial_cargo;
  VehicleStrategy strategy; // delivery, pickup, random
  std::list<ExtraFrame>::iterator frameIt; //the particular extraframe at the host
  MapPoint framePt; //index of the maptile with the frame, NOT necessarily the current position

  int speed0, speed, anim;
  void update(unsigned long real_time);
  bool isDead() const { return dead; }

  // how long the "arriving" animation lasts (milliseconds)
  static const unsigned long ARRIVE_MS = 500;
  // how long a blocked car waits (with retries) before giving up (ms)
  static const unsigned long BLOCK_TIMEOUT = 10000;
  // when the current wait (blocked) period expires; real_time based
  unsigned long wait_until;

  //pick a random passenger-car model among the 5 color variants
  static VehicleModel randomPassengerCar();
private:
  void getNewHeadings(); //plan ahead for 2 tiles
  bool acceptable_heading(MapPoint dest); //checks if a move would comply with the strategy
  void pickDestination(); //choose a nearby building as the trip destination
  bool tileOccupied(MapPoint p) const; //true if another vehicle already occupies the tile
  MapPoint travelDelta() const; //unit direction the vehicle is moving towards
  int buildingDirection(MapPoint p) const; //which neighbor of p is a building (0=N,1=S,2=E,3=W, -1 none)
  void drive();          //advance position by 1 tile
  void walk(unsigned long real_time);           //change the offset of the sprite and evetually choose a tile to attach it to
  void move_frame(MapPoint newPoint); //place the frame on the map aka *world(idx)
};

#endif
