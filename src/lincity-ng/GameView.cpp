/* ---------------------------------------------------------------------- *
 * src/lincity-ng/GameView.cpp
 * This file is part of Lincity-NG.
 *
 * Copyright (C) 2005      Wolfgang Becker <uafr@gmx.de>
 * Copyright (C) 2024-2025 David Bears <dbear4q@gmail.com>
 * Copyright (C) 2026      Marc Young <myoung008@gmail.com>
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

#include "GameView.hpp"

#include <SDL3/SDL.h>                     // for SDL_Scancode, SDL_BUTTON_LEFT
#include <SDL3_image/SDL_image.h>         // for IMG_Load
#include <assert.h>                       // for assert
#include <algorithm>                      // for stable_sort
#include <fmt/base.h>                     // for println
#include <fmt/format.h>                   // for format
#include <libxml++/parsers/textreader.h>  // for TextReader
#include <libxml++/ustring.h>             // for ustring
#include <cstdio>                         // for stderr
#include <cmath>                          // for sqrt, fabs, fabsf, floorf
#include <cstddef>                        // for size_t, NULL
#include <exception>                      // for exception
#include <functional>                     // for bind, _1, function
#include <iostream>                       // for operator<<, basic_ostream
#include <iterator>                       // for advance
#include <list>                           // for _List_iterator, list, opera...
#include <map>                            // for map, operator!=
#include <sstream>                        // for basic_stringstream
#include <stdexcept>                      // for runtime_error
#include <utility>                        // for pair
#include <vector>                         // for vector
#include <fmt/std.h> // IWYU pragma: keep

#include "Config.hpp"                     // for getConfig, Config
#include "Dialog.hpp"                     // for blockingDialogIsOpen
#include "Game.hpp"                       // for Game
#include "MiniMap.hpp"                    // for MiniMap, getMiniMap
#include "Mps.hpp"                        // for MpsMap
#include "Util.hpp"                       // for getButton, getParagraph
#include "gui/Button.hpp"                 // for Button
#include "gui/Color.hpp"                  // for Color
#include "gui/ComponentFactory.hpp"       // for IMPLEMENT_COMPONENT_FACTORY
#include "gui/Desktop.hpp"                // for Desktop
#include "gui/Document.hpp"               // for Document
#include "gui/Event.hpp"                  // for Event
#include "gui/Painter.hpp"                // for Painter
#include "gui/Paragraph.hpp"              // for Paragraph
#include "gui/Rect2D.hpp"                 // for Rect2D
#include "gui/Signal.hpp"                 // for Signal
#include "gui/Texture.hpp"                // for Texture
#include "gui/TextureManager.hpp"         // for TextureManager, texture_man...
#include "lincity-ng/UserOperation.hpp"   // for UserOperation
#include "lincity/all_buildings.hpp"      // for GROUP_WATER_BUL_COST, GROUP...
#include "lincity/groups.hpp"             // for GROUP_DESERT, GROUP_WATER
#include "lincity/lin-city.hpp"           // for FLAG_POWER_CABLES_0, FLAG_P...
#include "lincity/lintypes.hpp"           // for ConstructionGroup, Construc...
#include "lincity/messages.hpp"           // for Message
#include "lincity/modules/tile.hpp"       // for TileConstructionGroup, bare...
#include "lincity/modules/track_road_rail.hpp"  // for transport_group_at
#include "lincity/transport.hpp"          // for BRIDGE_FACTOR
#include "lincity/util.hpp"               // for format_money
#include "lincity/world.hpp"              // for Map, World, MapTile, Ground
#include "util/gettextutil.hpp"           // for _
#include "util/xmlutil.hpp"               // for xmlParse, unexpectedXmlAttr...

using namespace std::placeholders;


const int scale3d = 128; // guestimate value for good looking 3d view;

static bool isTransportGroup(const ConstructionGroup* group);

const float GameView::defaultTileWidth = 128;
const float GameView::defaultTileHeight = 64;
const float GameView::defaultZoom = 1.0;    // fastest drawing

GameView::GameView() {
    loaderThread = nullptr;
    keyScrollState = SCROLL_NONE;
    mouseScrollState = SCROLL_NONE;
    remaining_images = 0;
    textures_ready = false;

    // floating cost text near the cursor (FEAT-01)
      floatingDoc = std::make_unique<Document>();
    floatingParagraph = new Paragraph();
    floatingDoc->addParagraph(std::unique_ptr<Paragraph>(floatingParagraph));
    floatingDoc->style.background = Color(0, 0, 0, 170);
    floatingTextStyle.text_color = Color(255, 255, 255, 255);
}

GameView::~GameView() {
    stopThread.store(true, std::memory_order_release);
    if(loaderThread) {
      SDL_WaitThread(loaderThread, NULL);
      loaderThread = nullptr;
    }
}

//Static function to use with SDL_CreateThread
int GameView::gameViewThread( void* data )
{
    GameView* gv = (GameView*) data;
    try {
      gv->preReadImages();
    } catch(const std::exception& err) {
      gv->setTextureLoadError(fmt::format("{}", err.what()));
    } catch(...) {
      gv->setTextureLoadError("unknown exception while reading images");
    }
    gv->textures_ready.store(true, std::memory_order_release);
    //keep thread alive as long as there are SDL_Surfaces
    while(!gv->stopThread.load(std::memory_order_acquire)
      && gv->remaining_images.load(std::memory_order_acquire) != 0)
    {   SDL_Delay(100);}
    return 0;
}

void GameView::setTextureLoadError(const std::string& message) {
  if(!texture_load_failed.load(std::memory_order_relaxed)) {
    texture_load_error = message;
    texture_load_failed.store(true, std::memory_order_release);
    fmt::println(stderr, "error: texture loading failed: {}", message);
  }
}

void
GameView::parse(xmlpp::TextReader& reader) {
  while(reader.move_to_next_attribute()) {
    xmlpp::ustring name = reader.get_name();
    xmlpp::ustring value = reader.get_value();
    if(parseAttribute(reader));
    else
      unexpectedXmlAttribute(reader);
  }
  reader.move_to_element();

  blankGraphicsInfo.texture = readTexture( "blank.png" );
  blankGraphicsInfo.x = blankGraphicsInfo.texture->getWidth() / 2;
  blankGraphicsInfo.y = blankGraphicsInfo.texture->getHeight();

  stopThread.store(false, std::memory_order_release);
  textures_ready.store(false, std::memory_order_release);
  remaining_images.store(0, std::memory_order_release);
  texture_load_failed.store(false, std::memory_order_release);
  texture_load_error.clear();
  viewportStateInitialized = false;
  loaderThread = SDL_CreateThread( gameViewThread, "Loader", this );
  if(!loaderThread) {
    setTextureLoadError(fmt::format("could not start image loader: {}",
      SDL_GetError()));
    textures_ready.store(true, std::memory_order_release);
  }

  //GameView is resizable
  setFlags(FLAG_RESIZABLE);

  //start at location from savegame
  zoom = defaultZoom;
  tileWidth = defaultTileWidth * zoom;
  tileHeight = defaultTileHeight * zoom;

  mouseInGameView = false;
  dragging = false;
  leftButtonDown = false;
  roadDragging = false;
  ctrDrag = false;
  areaBulldoze = false;
  startRoad = MapPoint(0, 0);
  tileUnderMouse = MapPoint(0, 0);
  hideHigh = false;
  showTerrainHeight = false;
  cursorSize = 0;

  mapOverlay = overlayNone;
  buttonsConnected = false;
  lastStatusMessage = "";
  refreshMap = true;
}

void
GameView::setGame(Game *game) {
  this->game = game;
  game->getMiniMap().mapChanged.connect([this](){
    if(mapOverlay != overlayNone) {
      setDirty();
      setMapDirty();
    }
  });
}

World&
GameView::getWorld() const {
  return game->getWorld();
}

UserOperation *
GameView::getUserOperation() const {
  return &game->getUserOperation();
}

/*
 * Connect to GUI-Buttons.
 */
void GameView::connectButtons(){
    if( buttonsConnected ){
        return;
    }
    buttonsConnected = true;
    Component* root = this;
    while( root->getParent() ){
        root = root->getParent();
    }
    Button* button = getButton( *root, "hideHighBuildings" );
    button->clicked.connect(std::bind(&GameView::buttonClicked, this, _1));

    button = getButton( *root, "showTerrainHeight" );
    button->clicked.connect(std::bind(&GameView::buttonClicked, this, _1));

    button = getButton( *root, "mapOverlay" );
    button->clicked.connect(std::bind(&GameView::buttonClicked, this, _1));
}

/*
 * Handle Clicks on Button.
 */
void GameView::buttonClicked( Button* button ){
    std::string name = button->getName();
    if( name == "hideHighBuildings" ){
        hideHigh = !hideHigh;
        setDirty();
        setMapDirty();
        return;
    }
    if( name == "showTerrainHeight" ){
        if(getWorld().map.alt_step != 0) {
            showTerrainHeight = !showTerrainHeight;
        } else { // map is completely flat
            showTerrainHeight = false;
        }
        setDirty();
        return;
    }
    if( name == "mapOverlay" ){
        mapOverlay = (mapOverlay + 1) % (overlayMAX + 1);
        setDirty();
        setMapDirty();
        return;
    }
    std::cerr << "GameView::buttonClicked# Unhandled Button '" << name <<"',\n";
}

/*
 * size in Tiles of marking under Cursor
 * atm 0 is an outlined Version of size 1.
 */
void GameView::setCursorSize( int size )
{
    if( size != cursorSize )
    {
        cursorSize = size;
        setDirty();
    }
}

/*
 * evaluate main_screen_originx and main_screen_originy
 */
void GameView::readOrigin( bool redraw /* = true */ ) {
  show(getWorld().map.recentPoint, redraw);
}

/*
 * set main_screen_originx and main_screen_originy
 */
void GameView::writeOrigin() {
  getWorld().map.recentPoint = getCenter().x;
}

/*
 *  Get Tile in Center of Screen.
 */
MapPoint GameView::getCenter(){
    Vector2 center( getWidth() / 2, getHeight() / 2 );
    return getTile( center );
}

/*
 * Adjust the Zoomlevel.
 */
void GameView::setZoom(float newzoom){
    MapPoint centerTile  = getCenter();

    //if ( newzoom < .0625 ) return;
    if ( newzoom < .0312 ) return;
    if ( newzoom > 4 ) return;

    zoom = newzoom;

    // fix rounding errors...
    if(fabs(zoom - 1.0) < .01)
    {   zoom = 1;}

    tileWidth = defaultTileWidth * zoom;
    tileHeight = defaultTileHeight * zoom;

    //Show the Center
    show( centerTile );
}

void GameView::zoomMouse(float factor, Vector2 mousepos) {
    float newzoom = zoom * factor;

    //if ( newzoom < .0625 ) return;
    if (newzoom < .0312) {
        newzoom = .0312;
        factor = newzoom / zoom;
    }
    if(newzoom > 4) {
        newzoom = 4;
        factor = newzoom / zoom;
    }
    if(fabs(newzoom - 1.0) < .01) {
      // fix rounding errors...
      newzoom = 1;
      factor = newzoom / zoom;
    }

    zoom = newzoom;

    tileWidth = defaultTileWidth * zoom;
    tileHeight = defaultTileHeight * zoom;

    viewport = (viewport + mousepos) * factor - mousepos;
    constrainViewportPosition(true);

    viewportUpdated();
    setDirty();
}

/* set Zoomlevel to 100% */
void GameView::resetZoom(){
    setZoom( defaultZoom );
}

/* increase Zoomlevel */
void GameView::zoomIn(){
    setZoom( zoom * sqrt( 2.f ) );
}

/** decrease Zoomlevel */
void GameView::zoomOut(){
    setZoom( zoom / sqrt( 2.f ) );
}

/**
 *  Show City Tile(x/y) by centering the screen
 *  redraw = false is used on initialisation.
 */
void GameView::show( MapPoint map , bool redraw /* = true */ )
{
    Vector2 center;
    center.x = ( map.x - map.y ) * ( tileWidth / 2 );
    center.y = ( map.x + map.y ) * ( tileHeight / 2 ) + ( tileHeight / 2 );
    if( redraw ){
        viewport.x = center.x - ( getWidth() / 2 );
        viewport.y = center.y - ( getHeight() / 2 );
        viewportUpdated();
        setDirty();
    } else { //on startup getWidth is 0.
        viewport.x = center.x - ( getConfig()->videoX.get() / 2 );
        viewport.y = center.y - ( getConfig()->videoY.get() / 2 );
    }
}

/*
 * Loads Texture from filename, Returns Pointer to Texture
 * or Null if no file found. NOT THREADSAFE
 */

Texture* GameView::readTexture(const std::filesystem::path& filename) {
  Texture* currentTexture;
  try {
    currentTexture = texture_manager->load(
      std::filesystem::path("images") / "tiles" / filename);
  } catch(std::exception& err) {
    std::cerr << filename << " GameView::readTexture# missing: "
      << err.what() << "\n";
    return 0;
  }
  return currentTexture;
}

/*
 * Loads Image from filename, Returns Pointer to Image
 * or Null if no file found. IS THREADSAFE
 */
SDL_Surface* GameView::readImage(const std::filesystem::path& filename) {
  SDL_Surface* currentImage = IMG_Load((getConfig()->appDataDir.get() / "images" /
    "tiles" / filename).string().c_str());
  if( !currentImage ) {
    std::cerr << "GameView::readImage# Could not load image " << filename << "\n";
  }
  return currentImage;
}

// Recolor an image by its luminance: the shape/shadows are preserved but the
// color is replaced by the target tint. Used to produce colored vehicle
// variants from a single set of sprites. When no tint is requested the source
// is returned unchanged (no copy); otherwise a NEW surface is returned and the
// caller owns it (and keeps ownership of src).
// NOTE: never duplicate an untinted surface: SDL_DuplicateSurface drops the
// blend mode on indexed/palette surfaces (SDL 3.4.2), turning transparent
// pixels opaque black.
SDL_Surface* GameView::recolorSurface(SDL_Surface* src, const Tint& tint) {
  if(!src) return nullptr;
  if(!tint.enabled) return src;
  SDL_Surface* out = SDL_DuplicateSurface(src);
  if(!out) return nullptr;
  // keep the source's blend mode and color key in case the duplicate drops
  // them (seen on indexed/palette surfaces in SDL 3.4.2)
  SDL_BlendMode bm;
  if(SDL_GetSurfaceBlendMode(src, &bm))
    SDL_SetSurfaceBlendMode(out, bm);
  if(SDL_SurfaceHasColorKey(src)) {
    Uint32 key;
    if(SDL_GetSurfaceColorKey(src, &key))
      SDL_SetSurfaceColorKey(out, true, key);
  }
  SDL_LockSurface(out);
  for(int y = 0; y < out->h; ++y) {
    for(int x = 0; x < out->w; ++x) {
      Uint8 r, g, b, a;
      if(!SDL_ReadSurfacePixel(out, x, y, &r, &g, &b, &a))
        continue;
      if(a == 0) continue;
      // Rec.709 luma keeps brightness/shadows; color comes entirely from tint.
      int luma = (int)(0.2126f * r + 0.7152f * g + 0.0722f * b);
      r = (Uint8)((luma * tint.r) / 255);
      g = (Uint8)((luma * tint.g) / 255);
      b = (Uint8)((luma * tint.b) / 255);
      SDL_WriteSurfacePixel(out, x, y, r, g, b, a);
    }
  }
  SDL_UnlockSurface(out);
  return out;
}

/**
 * preload all images and with X and Y offsets.
 * from data/images/tiles/images.xml
 *
 *images and offsets are appended to ConstructionGroup::graphicVectorInfo see lintypes.h
 *the resourceID strings are hard coded in all_modules.cpp and match the names of the sounds
 *
 * some images contain the x-Coordinate of the
 * middle of the Building in Case the Image is asymetric,
 * eg. a high tower with a long shadow to the right
 *
 *  Some of the Image to Texture Conversion seems not to be threadsave
 *  in OpenGL, so load just Images and convert them to Textures on
 *  demand in the main Tread.
**/

void
GameView::preReadImages(void) {
  // TODO: Improve the format of images.xml so it doesn't rely on the order of
  //       elements.

  std::filesystem::path fullpath = getConfig()->appDataDir.get() /
    "images" / "tiles" / "images.xml";
  xmlpp::TextReader reader(fullpath.string());
  if(!reader.read())
    throw std::runtime_error(fmt::format("file is empty: {}", fullpath));
  while(reader.get_node_type() != xmlpp::TextReader::NodeType::Element) {
    if(!reader.next())
      throw std::runtime_error(
        fmt::format("file doesn't contain XML data: {}", fullpath));
  }

  ResourceGroup *resourceGroup = 0;
  int resourceID_level = 0;
  std::string key;

  if(!reader.is_empty_element())
  while(reader.read()) {
    if(reader.get_node_type() != xmlpp::TextReader::NodeType::Element) {
      continue;
    }
    const xmlpp::ustring element = reader.get_name();

    if(element == "resourceID") {
      std::string name;

      while(reader.move_to_next_attribute()) {
        xmlpp::ustring aname = reader.get_name();
        xmlpp::ustring value = reader.get_value();
        if(aname == "name")
          name = xmlParse<std::string>(value);
        else
          unexpectedXmlAttribute(reader);
      }
      reader.move_to_element();

      if(name.empty())
        missingXmlAttribute(reader, "name");

      if(resourceGroup)
        resourceGroup->images_loaded = true;

      if(ResourceGroup::resMap.count(name)) {
        resourceGroup = ResourceGroup::resMap[name];
        resourceID_level = reader.get_depth();
        if(resourceGroup->images_loaded) {
          fmt::println(stderr,
            "warning: duplicate resourceID in images.xml: {}", name);
          assert(false);
        }
      }
      else {
        fmt::println(stderr,
          "warning: unknown resourceID in images.xml: {}", name);
        resourceGroup = 0;
        resourceID_level = 0;
      }
    }
    //check if we are still inside context of last resorceID
    if(reader.get_depth() < resourceID_level-1) {
      resourceGroup->images_loaded = true;
      resourceGroup = 0;
      resourceID_level = 0;
    }
    if(element == "image") {
      int xmlX = 64;
      int xmlY = 32;
      bool hasX = false, hasY = false;

      while(reader.move_to_next_attribute()) {
        xmlpp::ustring name = reader.get_name();
        xmlpp::ustring value = reader.get_value();
        if(name == "file")
          key = xmlParse<std::string>(value);
        else if(name == "x") {
          xmlX = xmlParse<int>(value);
          hasX = true;
        }
        else if(name == "y") {
          xmlY = xmlParse<int>(value);
          hasY = true;
        }
        else
          unexpectedXmlAttribute(reader);
      }
      reader.move_to_element();

      if(resourceID_level && resourceGroup) {
        resourceGroup->growGraphicsInfoVector();
        GraphicsInfo *graphicsInfo = &(resourceGroup->graphicsInfoVector.back());
        SDL_Surface* loaded = readImage( key );
        // apply a color tint to produce e.g. differently colored car variants
        // from one set of sprites
        graphicsInfo->image = recolorSurface(loaded, resourceGroup->tint);
        if(graphicsInfo->image != loaded)
          SDL_DestroySurface(loaded);
        if(!graphicsInfo->image) {
          fmt::println(stderr, "error: failed to read image {}", key);
        }
        assert(hasX && hasY);
        if(!hasX)
          xmlX = graphicsInfo->image ? int(graphicsInfo->image->w/2) : 0;
        if(!hasY)
          xmlY = graphicsInfo->image ? int(graphicsInfo->image->h) : 0;
        graphicsInfo->x = xmlX;
        graphicsInfo->y = xmlY;
        // A missing/recoloring-failed image has nothing to convert. Do not
        // count it as pending or the loading barrier can never reach zero.
        if(graphicsInfo->image)
          ++remaining_images;
      }
      key.clear();
    }
  }

  while(reader.next()) {
    if(reader.get_node_type() != xmlpp::TextReader::NodeType::Element)
      continue;
    unexpectedXmlElement(reader);
  }

  if(resourceGroup)
  {
      resourceGroup->images_loaded = true;
      resourceGroup = 0;
  }
}


/*
 * Scroll the map.
 */
void GameView::scroll(float elapsedTime)
{
    int scrollState = keyScrollState | mouseScrollState;
    if( scrollState == SCROLL_NONE ) {
        return;
    }

    // QW-02: scroll speed is configurable in percent (100 = legacy speed).
    // The sqrt(zoom) makes it feel like the same speed at different zoom
    // levels.
    float amt = (elapsedTime * 1000) * 0.5 * sqrt(zoom)
      * (getConfig()->scrollSpeed.get() / 100.0);
    Vector2 dir = Vector2(0,0);

    if( keyScrollState & SCROLL_SHIFT_ALL ) {
        amt *= 4;
    }

    if( scrollState & SCROLL_UP_ALL ) {
        dir.y -= 1;
    }
    if( scrollState & SCROLL_DOWN_ALL ) {
        dir.y += 1;
    }
    if( scrollState & SCROLL_LEFT_ALL ) {
        dir.x -= 1;
    }
    if( scrollState & SCROLL_RIGHT_ALL ) {
        dir.x += 1;
    }

    if(dir == Vector2(0,0)) return;

    // The sqrt((float)tileWidth / tileHeight) makes vertical/horizonal
    // scrolling feel like the same speed. Surprisingly, without the square
    // root, it doesn't feel right.
    float norm = hypot(dir.x * sqrt((float)tileWidth / tileHeight), dir.y);
    // This makes diagonal scrolling parallel to map components.
    dir.x *= (float)tileWidth / tileHeight;
    viewport += dir * amt / norm;
    constrainViewportPosition(false);

    viewportUpdated();
    setDirty();
}

bool GameView::constrainViewportPosition(bool useScrollCorrection) {
  //If the centre of the Screen is not Part of the city
  //adjust viewport so it is.
  if(useScrollCorrection)
    viewport += scrollCorrection * zoom;
  Vector2 center = viewport + (Vector2(getWidth(), getHeight()) / 2);
  Vector2 centerTile = Vector2(
    center.y / tileHeight + center.x / tileWidth,
    center.y / tileHeight - center.x / tileWidth
  );
  bool outside = false;
  int mapSize = getWorld().map.len();
  if(centerTile.x < 1) {
      centerTile.x = 1;
      outside = true;
  }
  else if(centerTile.x > mapSize - 1) {
      centerTile.x = mapSize - 1;
      outside = true;
  }
  if(centerTile.y < 1) {
      centerTile.y = 1;
      outside = true;
  }
  else if(centerTile.y > mapSize - 1) {
      centerTile.y = mapSize - 1;
      outside = true;
  }

  if(outside) {
      Vector2 vpOld = viewport;
      center.x = ( centerTile.x - centerTile.y ) * tileWidth / 2;
      center.y = ( centerTile.x + centerTile.y ) * tileHeight / 2;
      viewport = center - (Vector2(getWidth(), getHeight()) / 2);
      if(useScrollCorrection)
        scrollCorrection = (vpOld - viewport) / zoom;
      else
        scrollCorrection = Vector2(0,0);
      viewportUpdated();
      setDirty();
      return true;
  }
  else {
    scrollCorrection = Vector2(0,0);
    return false;
  }
}

void
GameView::event(const Event& event) {
  switch(event.type) {
  case Event::MOUSEMOTION: {
    if(dragging) {
      viewport -= event.mousemove;
      constrainViewportPosition(true);
      viewportUpdated();
      setPanningCursor();
      setDirty();
      break;
    }
    else {
      scrollCorrection = Vector2(0,0);
    }

    mouseScrollState = SCROLL_NONE;
    if(event.mousepos.x < scrollBorder)
      mouseScrollState |= SCROLL_LEFT;
    else if(event.mousepos.x > getWidth() - scrollBorder)
      mouseScrollState |= SCROLL_RIGHT;

    if(event.mousepos.y < scrollBorder)
      mouseScrollState |= SCROLL_UP;
    else if(event.mousepos.y > getHeight() - scrollBorder)
      mouseScrollState |= SCROLL_DOWN;

    if(!event.inside) {
      mouseInGameView = false;
      break;
    }
    mouseInGameView = true;

    MapPoint tile = getTile(event.mousepos);
    if(!roadDragging && leftButtonDown && cursorSize == 1
      && getUserOperation()->action != UserOperation::ACTION_EVACUATE
    ) {
      roadDragging = true;
      startRoad = tile;
      areaBulldoze = (SDL_GetModState() & SDL_KMOD_CTRL);
    }

    if(roadDragging && cursorSize != 1) {
      roadDragging = false;
      areaBulldoze = false;
      ctrDrag = false;
    }
    // bulldoze at once while still dragging

    if(roadDragging && !areaBulldoze
      && getUserOperation()->action == UserOperation::ACTION_BULLDOZE
      && tile != startRoad
    ) {
      game->executeUserOperation(startRoad);
      startRoad = tile;
    }

    if(tileUnderMouse != tile) {
      tileUnderMouse = tile;
      setDirty();
      //update mps target
      if(getUserOperation()->action == UserOperation::ACTION_EVACUATE)
        game->getMpsMap().setTile(
          game->getWorld().map.is_visible(tile)
          ? game->getWorld().map(tile)
          : nullptr
        );
    }
  } break;
  case Event::MOUSEBUTTONDOWN: {
    if(!event.inside);
    else if(event.mousebutton == SDL_BUTTON_MIDDLE) {
      dragging = true;
      ctrDrag = false;
      mouseScrollState = SCROLL_NONE;
    }
    else if(event.mousebutton == SDL_BUTTON_LEFT) {
      roadDragging = false;
      areaBulldoze = false;
      ctrDrag = false;
      leftButtonDown = true;
    }
  } break;
  case Event::MOUSEBUTTONUP: {
    if(event.mousebutton == SDL_BUTTON_MIDDLE) {
      if(dragging) {
        dragging = false;
        setDefaultCursor();
        break;
      }
      dragging = false;
    }
    if(event.mousebutton == SDL_BUTTON_LEFT) {
      if(roadDragging && event.inside) {
        MapPoint endRoad = getTile(event.mousepos);
        roadDragging = false;
        areaBulldoze = false;
        leftButtonDown = false;
        if(cursorSize != 1)
          //roadDragging was aborted with Escape
          break;
        if(blockingDialogIsOpen)
          break;

        //build last tile first to play the sound
        Message::ptr dummyMsg;
        const int moneyBefore = game->getWorld().total_money;
        // check if allowed to avoid many dialogs for bulk ops
        if(getUserOperation()->isAllowedHere(
          game->getWorld(), endRoad, dummyMsg)
        ) {
          game->executeUserOperation(endRoad);
        }
        MapPoint currentTile = startRoad;
        int stepx = ( startRoad.x > endRoad.x ) ? -1 : 1;
        int stepy = ( startRoad.y > endRoad.y ) ? -1 : 1;
        if(getUserOperation()->action ==
          UserOperation::ACTION_BULLDOZE
        ) {
          for(currentTile.x = startRoad.x;
            currentTile.x != endRoad.x + stepx;
            currentTile.x += stepx
          )
          for(currentTile.y = startRoad.y;
            currentTile.y != endRoad.y + stepy;
            currentTile.y += stepy
          ) {
            if(getUserOperation()->isAllowedHere(
              game->getWorld(), currentTile, dummyMsg)
            ) {
              game->executeUserOperation(currentTile);
            }
          }
        }
        else if(getUserOperation()->action ==
          UserOperation::ACTION_BUILD
        ) {
          int* v1 = ctrDrag ? &currentTile.y : &currentTile.x;
          int* v2 = ctrDrag ? &currentTile.x : &currentTile.y;
          int* l1 = ctrDrag ? &endRoad.y : &endRoad.x;
          int* l2 = ctrDrag ? &endRoad.x : &endRoad.y;
          int* s1 = ctrDrag ? &stepy : &stepx;
          int* s2 = ctrDrag ? &stepx : &stepy;

          while(*v1 != *l1) {
            if(getUserOperation()->isAllowedHere(
              game->getWorld(), currentTile, dummyMsg)
            ) {
              game->executeUserOperation(currentTile);
            }
            *v1 += *s1;
          }
          while(*v2 != *l2) {
            if(getUserOperation()->isAllowedHere(
              game->getWorld(), currentTile, dummyMsg)
            ) {
              game->executeUserOperation(currentTile);
            }
            *v2 += *s2;
          }
        }
        // floating negative total for what was actually charged (FEAT-01)
        const int spent = moneyBefore - game->getWorld().total_money;
        if(spent > 0) {
          Vector2 pos = getScreenPoint(endRoad);
          pos.x += tileWidth * 0.5f;
          pos.y -= 14;
          setFloatingText("-" + format_money(spent) + "$", pos, 1600);
        }
        else {
          clearFloatingText();
        }
        break;
      }
      roadDragging = false;
      ctrDrag = false;
      areaBulldoze = false;
      leftButtonDown = false;
    }
    if(!event.inside)
      break;

    if(event.mousebutton == SDL_BUTTON_LEFT) {
      if(!blockingDialogIsOpen) {
        const MapPoint clickTile = getTile(event.mousepos);
        const int moneyBefore = game->getWorld().total_money;
        game->executeUserOperation(clickTile);
        // floating negative total for what was actually charged (FEAT-01)
        const int spent = moneyBefore - game->getWorld().total_money;
        if(spent > 0) {
          Vector2 pos = getScreenPoint(clickTile);
          pos.x += tileWidth * 0.5f;
          pos.y -= 14;
          setFloatingText("-" + format_money(spent) + "$", pos, 1600);
        }
      }
    }
    else if(event.mousebutton == SDL_BUTTON_RIGHT) {
      // show info on the clicked thing
      MapPoint point = getTile(event.mousepos);
      if(!inCity(point)) break;
      game->getMpsMap().query(game->getWorld().map(point));
      game->getMiniMap().switchView("MapMPS");
    }
  } break;
  case Event::MOUSEWHEEL: {
    if(!event.inside || event.scrolly == 0)
      break;
    if(event.scrolly > 0)
      zoomMouse(sqrt(2.f), event.mousepos);
    else
      zoomMouse(sqrt(0.5), event.mousepos);
  } break;
  case Event::WINDOWLEAVE: {
    mouseInGameView = false;
    mouseScrollState = SCROLL_NONE;
  } break;
  case Event::WINDOWENTER:
    break;
  case Event::KEYDOWN: {
    switch(event.scancode) {
    case SDL_SCANCODE_LCTRL:
    case SDL_SCANCODE_RCTRL:
      if(roadDragging)
        ctrDrag = !ctrDrag;
      break;
    case SDL_SCANCODE_KP_8:
    case SDL_SCANCODE_UP:
    case SDL_SCANCODE_W:
      keyScrollState |= SCROLL_UP;
      break;
    case SDL_SCANCODE_KP_2:
    case SDL_SCANCODE_DOWN:
    case SDL_SCANCODE_S:
      keyScrollState |= SCROLL_DOWN;
      break;
    case SDL_SCANCODE_KP_4:
    case SDL_SCANCODE_LEFT:
    case SDL_SCANCODE_A:
      keyScrollState |= SCROLL_LEFT;
      break;
    case SDL_SCANCODE_KP_6:
    case SDL_SCANCODE_RIGHT:
    case SDL_SCANCODE_D:
      keyScrollState |= SCROLL_RIGHT;
      break;
    case SDL_SCANCODE_KP_7:
      keyScrollState |= SCROLL_UP_LEFT;
      break;
    case SDL_SCANCODE_KP_9:
      keyScrollState |= SCROLL_UP_RIGHT;
      break;
    case SDL_SCANCODE_KP_1:
      keyScrollState |= SCROLL_DOWN_LEFT;
      break;
    case SDL_SCANCODE_KP_3:
      keyScrollState |= SCROLL_DOWN_RIGHT;
      break;
    case SDL_SCANCODE_LSHIFT:
      keyScrollState |= SCROLL_LSHIFT;
      break;
    case SDL_SCANCODE_RSHIFT:
      keyScrollState |= SCROLL_RSHIFT;
      break;
    case SDL_SCANCODE_N:
      getMiniMap()->scrollPageDown(true);
      break;
    case SDL_SCANCODE_M:
      getMiniMap()->scrollPageDown(false);
      break;
    }
  } break;
  case Event::KEYUP: {
    switch(event.scancode) {
    case SDL_SCANCODE_G: {
      MpsMap& mps = game->getMpsMap();
      if(mps.page == MpsMap::Page::GROUND)
        mps.page = MpsMap::Page::INVENTORY;
      else
        mps.page = MpsMap::Page::GROUND;
      mps.refresh();
      game->getMiniMap().switchView("MapMPS");
      break;
    }
    case SDL_SCANCODE_H: {
      hideHigh = !hideHigh;
      setDirty();
      setMapDirty();
    } break;
    case SDL_SCANCODE_V: {
      mapOverlay = (mapOverlay+1)%(overlayMAX+1);
      setDirty();
      setMapDirty();
    } break;
    //Zoom
    case SDL_SCANCODE_KP_PLUS:
      zoomIn();
      break;
    case SDL_SCANCODE_KP_MINUS:
      zoomOut();
      break;
    case SDL_SCANCODE_RETURN:
    case SDL_SCANCODE_RETURN2:
      resetZoom();
      break;
    //Scroll
    case SDL_SCANCODE_KP_8:
    case SDL_SCANCODE_UP:
    case SDL_SCANCODE_W:
      keyScrollState &= ~SCROLL_UP;
      break;
    case SDL_SCANCODE_KP_2:
    case SDL_SCANCODE_DOWN:
    case SDL_SCANCODE_S:
      keyScrollState &= ~SCROLL_DOWN;
      break;
    case SDL_SCANCODE_KP_4:
    case SDL_SCANCODE_LEFT:
    case SDL_SCANCODE_A:
      keyScrollState &= ~SCROLL_LEFT;
      break;
    case SDL_SCANCODE_KP_6:
    case SDL_SCANCODE_RIGHT:
    case SDL_SCANCODE_D:
      keyScrollState &= ~SCROLL_RIGHT;
      break;
    case SDL_SCANCODE_KP_7:
      keyScrollState &= ~SCROLL_UP_LEFT;
      break;
    case SDL_SCANCODE_KP_9:
      keyScrollState &= ~SCROLL_UP_RIGHT;
      break;
    case SDL_SCANCODE_KP_1:
      keyScrollState &= ~SCROLL_DOWN_LEFT;
      break;
    case SDL_SCANCODE_KP_3:
      keyScrollState &= ~SCROLL_DOWN_RIGHT;
      break;
    case SDL_SCANCODE_LSHIFT:
      keyScrollState &= ~SCROLL_LSHIFT;
      break;
    case SDL_SCANCODE_RSHIFT:
      keyScrollState &= ~SCROLL_RSHIFT;
      break;
    case SDL_SCANCODE_KP_5: {
      show(MapPoint(getWorld().map.len() / 2, getWorld().map.len() / 2));
      setDirty();
    } break;
    }
  } break;
  case Event::UPDATE: {
    scroll(event.elapsedTime);
  } break;
  default:
    break;
  }
}

void GameView::setPanningCursor() {
    desktop->setSystemCursor(this, SDL_SYSTEM_CURSOR_MOVE);
}

void GameView::setDefaultCursor() {
    desktop->tryClearCursor(this);
}

/*
 * Parent tells us to change size.
 */
void GameView::resize(float newwidth , float newheight )
{
    width = newwidth;
    height = newheight;
    if(width < 0) width = 0;
    if(height < 0) height = 0;
    viewportUpdated();
    setDirty();
}

/*
 *  We should draw the whole City again.
 */
void GameView::viewportUpdated()
{
    if( !getMiniMap() ){ //initialization not completed
        return;
    }

    MapPoint newCenter = this->getCenter();

    if(!viewportStateInitialized) {
      previousViewportCenter = newCenter;
      previousViewportZoom = zoom;
      viewportStateInitialized = true;
      refreshMap = true;
      return;
    }

    if ( (previousViewportCenter != newCenter)
      || (previousViewportZoom != zoom) ) {
      //Tell Minimap about new Corners
      Vector2 viewport2 = viewport + getSize();
      getMiniMap()->setGameViewCorners(
        Vector2(
          viewport.y / tileHeight + viewport.x / tileWidth,
          viewport.y / tileHeight - viewport.x / tileWidth
        ),
        Vector2(
          viewport2.y / tileHeight + viewport2.x / tileWidth,
          viewport2.y / tileHeight - viewport2.x / tileWidth
        )
      );

      previousViewportCenter = newCenter;
      previousViewportZoom = zoom;
    }

    refreshMap = true;
}

/*
 * Find point on Screen, where lower right corner of tile
 * is placed.
 */
Vector2 GameView::getScreenPoint(MapPoint mp)
{
    Vector2 point;
    point.x = (mp.x - mp.y) * ( tileWidth / 2 );
    point.y = (mp.x + mp.y) * ( tileHeight / 2 );

    //we want the lower right corner
    point.y += tileHeight;

    if ((showTerrainHeight) && (inCity(mp))){
        // shift the tile upward to show altitude
        point.y -= (float) ( (getWorld().map(mp)->ground.altitude) * scale3d) * zoom  / (float) getWorld().map.alt_step ;
    }

    //on Screen
    point -= viewport;

    return point;
}

/*
 * Find Tile at point on viewport
 */
MapPoint GameView::getTile(const Vector2& p)
{
    MapPoint tile;
    // Map Point to virtual Screen
    Vector2 point = p + viewport;
    tile.x = (int) floorf(point.y / tileHeight + point.x / tileWidth);
    tile.y = (int) floorf(point.y / tileHeight - point.x / tileWidth);

    return tile;
}

/*
 * Draw a filled Diamond inside given Rectangle
 */
void GameView::fillDiamond( Painter& painter, const Rect2D& rect )
{
    Vector2 points[ 4 ];
    points[ 0 ].x = rect.p1.x + ( rect.getWidth() / 2 );
    points[ 0 ].y = rect.p1.y;
    points[ 1 ].x = rect.p1.x;
    points[ 1 ].y = rect.p1.y + ( rect.getHeight() / 2 );
    points[ 2 ].x = rect.p1.x + ( rect.getWidth() / 2 );
    points[ 2 ].y = rect.p2.y;
    points[ 3 ].x = rect.p2.x;
    points[ 3 ].y = rect.p1.y + ( rect.getHeight() / 2 );
    painter.fillPolygon( 4, points );
}

/*
 * Draw a outlined Diamond inside given Rectangle
 */
void GameView::drawDiamond( Painter& painter, const Rect2D& rect )
{
    if( !buttonsConnected ) connectButtons();//has to be done here,
                    // because now everything else is initialized.
    Vector2 points[ 4 ];
    points[ 0 ].x = rect.p1.x + ( rect.getWidth() / 2 );
    points[ 0 ].y = rect.p1.y;
    points[ 1 ].x = rect.p1.x;
    points[ 1 ].y = rect.p1.y + ( rect.getHeight() / 2 );
    points[ 2 ].x = rect.p1.x + ( rect.getWidth() / 2 );
    points[ 2 ].y = rect.p2.y;
    points[ 3 ].x = rect.p2.x;
    points[ 3 ].y = rect.p1.y + ( rect.getHeight() / 2 );
    painter.drawPolygon( 4, points );
}

/*
 * Check if Tile is in City
 * in oldgui you can edit (1,1) to (98,98) with world.len() 100
 * i.e. there is a hidden border of green tiles arround the city
 */
bool GameView::inCity( MapPoint tile )
{
    return getWorld().map.is_visible(tile);
}

/*
 * Draw MiniMapOverlay for tile.
 */
void GameView::drawOverlay(Painter& painter, const MapPoint &tile){
    Color black;
    black.parse("black");
    Color miniMapColor;

    Vector2 tileOnScreenPoint = getScreenPoint(tile);
    Rect2D tilerect( 0, 0, tileWidth, tileHeight );
    tileOnScreenPoint.x = tileOnScreenPoint.x - ( tileWidth / 2);
    tileOnScreenPoint.y -= tileHeight;
    tilerect.move( tileOnScreenPoint );
    //Outside of the Map gets Black overlay
    if( !inCity( tile ) ) {
            painter.setFillColor( black );
    } else {
        miniMapColor = getMiniMap()->getColor(*getWorld().map(tile));
        if( mapOverlay == overlayOn ){
            miniMapColor.a = 200;  //Transparent
        }
        painter.setFillColor( miniMapColor );
    }
    fillDiamond( painter, tilerect );
}

/*
 * If the current Tile is Part of a Building, return the
 * Coordinates of the tile that contains the real informations.
 */
MapPoint GameView::realTile(MapPoint point) {
  if(!inCity(point))
    return point;
  else if(getWorld().map(point)->reportingConstruction)
    return getWorld().map(point)->reportingConstruction->point;
  else
    return point;
}

void GameView::fetchTextures() {
  std::map<std::string, ResourceGroup*>::iterator it;
  for(it= ResourceGroup::resMap.begin(); it != ResourceGroup::resMap.end(); ++it) {
    for(size_t i = 0; i < it->second->graphicsInfoVector.size(); ++i) {
      auto& gfx = it->second->graphicsInfoVector[i];
      if(!gfx.texture && gfx.image) {
        try {
          gfx.texture = texture_manager->create(gfx.image);
        } catch(const std::exception& err) {
          setTextureLoadError(err.what());
          return;
        } catch(...) {
          setTextureLoadError("unknown exception while creating texture");
          return;
        }
        if(gfx.texture) {
          // BUG-10: texture_manager->create() does NOT destroy the source
          // surface (TextureManagerSDL::create only destroys its own mipmap
          // intermediates). The old comment "Image was erased by
          // texture_manager->create" was wrong, so the surface and its
          // pixel buffer leaked every time an image was converted to a
          // texture. Destroy it here.
          SDL_DestroySurface(gfx.image);
          gfx.image = 0;
          gfx.texture->setScaleMode(Texture::ScaleMode::NEAREST);
          --remaining_images;
        } else {
          setTextureLoadError("texture manager returned null");
          return;
        }
      }
    }
  }
}



void GameView::drawTexture(Painter& painter, const MapPoint &tile, GraphicsInfo *graphicsInfo)
{
    Rect2D tilerect( 0, 0, tileWidth, tileHeight );
    Vector2 tileOnScreenPoint = getScreenPoint( tile );
    // Test if we have to convert Preloaded Image to Texture
    if( !graphicsInfo->texture ) //&& !economyGraph_open
    {
        if(graphicsInfo->image)
        {
            try {
              graphicsInfo->texture = texture_manager->create(
                graphicsInfo->image);
            } catch(const std::exception& err) {
              setTextureLoadError(err.what());
              return;
            } catch(...) {
              setTextureLoadError("unknown exception while creating texture");
              return;
            }
            if(graphicsInfo->texture) {
              // BUG-10: same as fetchTextures — create() does not destroy
              // the source surface. Destroy it here.
              SDL_DestroySurface(graphicsInfo->image);
              graphicsInfo->image = 0;
              graphicsInfo->texture->setScaleMode(Texture::ScaleMode::NEAREST);
              --remaining_images;
            } else {
              setTextureLoadError("texture manager returned null");
            }
        }
    }
    if (graphicsInfo->texture)
    {
        tileOnScreenPoint.x -= graphicsInfo->x * zoom;
        tileOnScreenPoint.y -= graphicsInfo->y * zoom;
        tilerect.move( tileOnScreenPoint );
        tilerect.setSize(graphicsInfo->texture->getWidth() * zoom,
            graphicsInfo->texture->getHeight() * zoom);
        painter.drawStretchTexture(graphicsInfo->texture, tilerect);
    }
}


void GameView::drawTile(Painter& painter, const MapPoint &tile,
    std::vector<GameView::RenderFrame>& frameQueue,
    std::vector<MapPoint>& cableTiles)
{

    //is Tile in City? If not draw Blank
    if( ! inCity( tile ) )
    {
        drawTexture(painter, tile, &blankGraphicsInfo);
        return;
    }

    //Texture* texture = 0;
    MapPoint upperLeft = realTile(tile);

    ConstructionGroup *cstgrp =
      getWorld().map(upperLeft)->getTopConstructionGroup();
    ResourceGroup *resgrp;
    unsigned short size = cstgrp->size;

    //Attention map is rotated for displaying
    if(tile.n(size-1) == upperLeft) //Signs are tested
    {
        resgrp = getWorld().map(upperLeft)->getTileResourceGroup();
        //adjust OnScreenPoint of big Tiles
        MapPoint lowerRightTile(tile.e(size-1));
        unsigned short textureType = getWorld().map(upperLeft)->getTopType();

        // if we hide high buildings, hide trees as well
        if (hideHigh && (cstgrp == &treeConstructionGroup
         || cstgrp == &tree2ConstructionGroup
         || cstgrp == &tree3ConstructionGroup ))
        {
            cstgrp = &bareConstructionGroup;
            resgrp = ResourceGroup::resMap["Green"];
        }
        GraphicsInfo *graphicsInfo = 0;
        //draw visible tiles underneath constructions
        if(Construction *cst = getWorld().map(upperLeft)->reportingConstruction;
          !cst || (cst->flags & FLAG_TRANSPARENT))
        {
            if (resgrp->images_loaded)
            {
                size_t s = resgrp->graphicsInfoVector.size();
                if (s)
                {
                    graphicsInfo = &resgrp->graphicsInfoVector
                        [ getWorld().map(upperLeft)->type  % s];
                    drawTexture(painter, lowerRightTile, graphicsInfo);
                }
            }
        }
        bool draw_colored_site = true;

        if( (size==1 || !hideHigh) )
        {
            draw_colored_site = false;
            if (getWorld().map(upperLeft)->hasFrames())
            {
                std::list<ExtraFrame>& tileFrames = getWorld().map(upperLeft)->frames();
                for(std::list<ExtraFrame>::iterator frit = tileFrames.begin();
                    frit != tileFrames.end(); std::advance(frit, 1))
                {
                    if(frit->resourceGroup && frit->resourceGroup->images_loaded)
                    {
                        size_t s2 = frit->resourceGroup->graphicsInfoVector.size();
                        if((frit->frame >= 0) && s2)
                        {
                            graphicsInfo = &frit->resourceGroup->graphicsInfoVector[ frit->frame % s2 ]; //needed
                            int old_x = graphicsInfo->x;
                            int old_y = graphicsInfo->y;
                            graphicsInfo->x = old_x - frit->move_x;
                            graphicsInfo->y = old_y - frit->move_y;
                            const bool vehicle = frit->resourceGroup->is_vehicle;
                            bool vehicleAllowed = true;
                            if(vehicle)
                            {
                                // Vehicle sprites use the road tile as their
                                // visual layer, but never a building tile.
                                switch(cstgrp->group)
                                {
                                    case GROUP_BARE:
                                    case GROUP_DESERT:
                                    case GROUP_WATER:
                                    case GROUP_TRACK:
                                    case GROUP_TRACK_BRIDGE:
                                    case GROUP_ROAD:
                                    case GROUP_ROAD_BRIDGE:
                                    case GROUP_RAIL:
                                    case GROUP_RAIL_BRIDGE:
                                    case GROUP_ORGANIC_FARM:
                                    case GROUP_WATERWELL:
                                    case GROUP_SHANTY:
                                    case GROUP_CRICKET:
                                    case GROUP_SUBSTATION:
                                        break;
                                    default:
                                        vehicleAllowed = false;
                                        break;
                                }
                            }

                            if(!vehicle || vehicleAllowed)
                            {
                                const bool transportBase = !vehicle
                                    && isTransportGroup(cstgrp);
                                const double depth = vehicle
                                    ? upperLeft.x + upperLeft.y
                                      + frit->move_y
                                        / (defaultTileHeight / 2.0)
                                    : lowerRightTile.x + lowerRightTile.y;
                                frameQueue.push_back({
                                    &*frit,
                                    vehicle ? upperLeft : lowerRightTile,
                                    depth,
                                    vehicle ? 1 : 2,
                                    transportBase
                                });
                            }
                            graphicsInfo->x = old_x;
                            graphicsInfo->y = old_y;
                        }
                    }
                    else
                    {   draw_colored_site = true;}
                }
            }
        }

        if(draw_colored_site)
        {
            Rect2D tilerect( 0, 0, size * tileWidth, size * tileHeight );
            Vector2 tileOnScreenPoint = getScreenPoint( lowerRightTile );
            tileOnScreenPoint.x =  tileOnScreenPoint.x - ( tileWidth*size / 2);
            tileOnScreenPoint.y -= tileHeight*size;
            tilerect.move( tileOnScreenPoint );
            painter.setFillColor(getMiniMap()->getColorNormal(*getWorld().map(tile)));
            fillDiamond( painter, tilerect );
        }
        // Suspended power cables are drawn after the depth-sorted frames.
        // This only works for size == 1.
        if (getWorld().map(upperLeft)->flags & (FLAG_POWER_CABLES_0 | FLAG_POWER_CABLES_90))
        {
            cableTiles.push_back(upperLeft);
        }
    }


}

/*
 * Mark a tile with current cursor
 */
void GameView::markTile(Painter& painter, MapPoint point) {
    Vector2 tileOnScreenPoint = getScreenPoint(point);
    {
      MapPoint upperLeft = realTile(point);
      Construction *cst;
      if(game->getMpsMap().tile
        && upperLeft == realTile(game->getMpsMap().tile->point)
        && getUserOperation()->action == UserOperation::ACTION_QUERY
        && (cst = getWorld().map(point)->reportingConstruction)
      ) {
        ConstructionGroup *constructionGroup = cst->constructionGroup;
        int range = constructionGroup->range;
        int edgelen = 2 * range + constructionGroup->size;
        painter.setFillColor(Color(0, 255, 0, 64));
        Rect2D rangerect(0, 0, tileWidth * edgelen, tileHeight * edgelen);
        Vector2 screenPoint = getScreenPoint(upperLeft);
        screenPoint.x -= tileWidth * (0.5*edgelen);
        screenPoint.y -= tileHeight * (range + 1);
        rangerect.move(screenPoint);
        fillDiamond(painter, rangerect);
      }
    }

    if( getUserOperation()->action == UserOperation::ACTION_QUERY) //  cursorSize == 0
    {
        Color alphawhite( 255, 255, 255, 128 );
        painter.setLineColor( alphawhite );
        Rect2D tilerect( 0, 0, tileWidth, tileHeight );
        tileOnScreenPoint.x -= ( tileWidth / 2);
        tileOnScreenPoint.y -= tileHeight;
        tilerect.move( tileOnScreenPoint );
        drawDiamond( painter, tilerect );
    }
    else
    {
        Color alphablue( 0, 0, 255, 128 );
        Color alphared( 255, 0, 0, 128 );
        Message::ptr tmpMsg;
        if(getUserOperation()->isAllowedHere(getWorld(), point, tmpMsg))
          painter.setFillColor(alphablue);
        else
          painter.setFillColor(alphared);

        Rect2D tilerect( 0, 0, tileWidth * cursorSize, tileHeight * cursorSize );
        tileOnScreenPoint.x -= (tileWidth * cursorSize / 2);
        tileOnScreenPoint.y -= tileHeight;
        tilerect.move( tileOnScreenPoint );
        fillDiamond( painter, tilerect );

        if(getUserOperation()->action == UserOperation::ACTION_BUILD)
        {
            // Draw range for selected_building
            int range = getUserOperation()->constructionGroup->range;
            if (range > 0 )
            {
                int edgelen = 2 * range + getUserOperation()->constructionGroup->size ;
                painter.setFillColor( Color( 0, 0, 128, 64 ) );
                Rect2D rangerect( 0,0,
                                  tileWidth  * ( edgelen) ,
                                  tileHeight * ( edgelen) );
                Vector2 screenPoint = getScreenPoint(point);
                screenPoint.x -= tileWidth  * ( 0.5*edgelen );
                screenPoint.y -= tileHeight * ( range + 1 );
                rangerect.move( screenPoint );
                fillDiamond( painter, rangerect );
            }//endif range > 0
        }
    }//endelse ACTION_QUERY
}

/*
 *  Paint an isometric View of the City in the component.
 */
void GameView::draw(Painter& painter)
{
    // Constraining the position here shouldn't be necessary if other parts of the code are correct.
    // if( constrainViewportPosition(true) ) {
    //     // Returning causes the display to lag. I'm not sure why it's needed anyway.
    //     // return;
    // }

    //The Corners of The Screen
    Vector2 upperLeft( 0, 0);
    Vector2 upperRight( getWidth(), 0 );
    Vector2 lowerLeft( 0, getHeight() );

    if (showTerrainHeight)
    {
        // printf("h = %f,     z = %f \n ", getHeight(), zoom);
        // getHeight = size in pixel of the screen (eg 1024x768)
        Vector2 lowerLeft( 0, getHeight() * ( 1 + getHeight() * zoom / (float)scale3d ));
    }

    //Find visible Tiles
    MapPoint upperLeftTile  = getTile( upperLeft );
    MapPoint upperRightTile = getTile( upperRight );
    MapPoint lowerLeftTile  = getTile( lowerLeft );

    //draw Background
    Color black;
    black.parse("black");
    painter.setFillColor(black);
    painter.clear();

    //draw Tiles
    MapPoint currentTile;
    //Draw some extra tiles depending on the maximal size of a building.
    int extratiles = 7;
    upperLeftTile.x -= extratiles;
    upperRightTile.y -= extratiles;
    upperRightTile.x += extratiles;
    lowerLeftTile.y +=  extratiles;

    if(!mapTexture || refreshMap) {
      if(!mapTexture
        || mapTexture->getWidth() != (int)getWidth()
        || mapTexture->getHeight() != (int)getHeight()
      ) {
        mapTexture = painter.createTargetTexture(getWidth(), getHeight());
      }

      painter.pushRenderTarget(mapTexture.get());
      painter.setFillColor(Color(0,0,0));
      painter.clear();

      if(mapOverlay != overlayOnly) {
        std::vector<GameView::RenderFrame> frameQueue;
        std::vector<MapPoint> cableTiles;
        for(int k = 0; k <= 2 * (lowerLeftTile.y - upperLeftTile.y); k++)
        for(int i = 0; i <= upperRightTile.x - upperLeftTile.x; i++) {
          currentTile.x = upperLeftTile.x + i + k / 2 + k % 2;
          currentTile.y = upperLeftTile.y - i + k / 2;
          drawTile(painter, currentTile, frameQueue, cableTiles);
        }

        std::stable_sort(frameQueue.begin(), frameQueue.end(),
          [](const GameView::RenderFrame& a,
              const GameView::RenderFrame& b) {
            if(a.transportBase != b.transportBase)
              return a.transportBase > b.transportBase;
            if(a.depth != b.depth)
              return a.depth < b.depth;
            return a.layer < b.layer;
          });

        for(const GameView::RenderFrame& renderFrame : frameQueue) {
          ResourceGroup* resourceGroup = renderFrame.frame->resourceGroup;
          if(!resourceGroup || !resourceGroup->images_loaded)
            continue;
          size_t size = resourceGroup->graphicsInfoVector.size();
          if(renderFrame.frame->frame < 0 || !size)
            continue;
          GraphicsInfo* graphicsInfo = &resourceGroup->graphicsInfoVector[
            renderFrame.frame->frame % size];
          int old_x = graphicsInfo->x;
          int old_y = graphicsInfo->y;
          graphicsInfo->x = old_x - renderFrame.frame->move_x;
          graphicsInfo->y = old_y - renderFrame.frame->move_y;
          drawTexture(painter, renderFrame.drawTile, graphicsInfo);
          graphicsInfo->x = old_x;
          graphicsInfo->y = old_y;
        }

        ResourceGroup* powerLine = ResourceGroup::resMap["PowerLine"];
        if(powerLine->images_loaded) {
          for(const MapPoint& cableTile : cableTiles) {
            if(getWorld().map(cableTile)->flags & FLAG_POWER_CABLES_0)
              drawTexture(painter, cableTile,
                &powerLine->graphicsInfoVector[23]);
            if(getWorld().map(cableTile)->flags & FLAG_POWER_CABLES_90)
              drawTexture(painter, cableTile,
                &powerLine->graphicsInfoVector[22]);
          }
        }
       }
      if(mapOverlay != overlayNone) {
        for(int k = 0; k <= 2 * (lowerLeftTile.y - upperLeftTile.y); k++)
        for(int i = 0; i <= upperRightTile.x - upperLeftTile.x; i++) {
          currentTile.x = upperLeftTile.x + i + k / 2 + k % 2;
          currentTile.y = upperLeftTile.y - i + k / 2;
          drawOverlay(painter, currentTile);
        }
      }
      painter.popRenderTarget();
      refreshMap = false;
    }
    painter.drawTexture(mapTexture.get(), Vector2(0,0));

    // When placing a building, highlight the coverage range of every
    // existing building of the same type that is visible, so the user can
    // position the new one to cover as much ground as possible (#242).
    if(getUserOperation()->action == UserOperation::ACTION_BUILD) {
      ConstructionGroup *selGroup = getUserOperation()->constructionGroup;
      int range = selGroup->range;
      if(range > 0) {
        int edgelen = 2 * range + selGroup->size;
        int visMinX = upperLeftTile.x;
        int visMaxX = upperRightTile.x;
        int visMinY = upperLeftTile.y;
        int visMaxY = lowerLeftTile.y;
        painter.setFillColor(Color(0, 255, 0, 48));
        for(Construction *cst : getWorld().map.constructions) {
          if(cst->isDead()) continue;
          if(cst->constructionGroup != selGroup) continue;
          MapPoint c = cst->point;
          if(c.x < visMinX - edgelen || c.x > visMaxX + edgelen
            || c.y < visMinY - edgelen || c.y > visMaxY + edgelen)
            continue;
          Rect2D rangerect(0, 0, tileWidth * edgelen, tileHeight * edgelen);
          Vector2 screenPoint = getScreenPoint(c);
          screenPoint.x -= tileWidth * (0.5 * edgelen);
          screenPoint.y -= tileHeight * (range + 1);
          rangerect.move(screenPoint);
          fillDiamond(painter, rangerect);
        }
      }
    }

    int cost = 0;
    long long landCost = 0, bridgeCost = 0;
    int landTiles = 0, bridgeTiles = 0;
    //display commodities continously
    if(getUserOperation()->action == UserOperation::ACTION_EVACUATE) {
      game->getMpsMap().refresh();
      game->getMiniMap().switchView("MapMPS");
    }
    //Mark Tile under Mouse // TODO: handle showTerrainHeight
    if(mouseInGameView && !blockingDialogIsOpen) {
        MapPoint lastRazed( -1,-1 );
        int tiles = 0;
        const bool previewBuild =
          getUserOperation()->action == UserOperation::ACTION_BUILD;
        const bool previewBulldoze =
          getUserOperation()->action == UserOperation::ACTION_BULLDOZE;
        ConstructionGroup* previewGroup =
          getUserOperation()->constructionGroup;
        if( roadDragging && ( cursorSize == 1 ) &&
        (previewBuild || previewBulldoze))
        {
            //use same method to find all Tiles as in GameView::event(const Event& event)
            int stepx = ( startRoad.x > tileUnderMouse.x ) ? -1 : 1;
            int stepy = ( startRoad.y > tileUnderMouse.y ) ? -1 : 1;
            currentTile = startRoad;

            if (previewBulldoze)
            {
                for (;currentTile.x != tileUnderMouse.x + stepx; currentTile.x += stepx) {
                    for (currentTile.y = startRoad.y; currentTile.y != tileUnderMouse.y + stepy; currentTile.y += stepy) {
                        markTile( painter, currentTile );
                        if( realTile( currentTile ) != lastRazed ){
                            cost += bulldozeCost( currentTile );
                            lastRazed = realTile( currentTile );
                        }
                        tiles++;
                    }
                }
            }
            else if (previewBuild)
            {
                // Recompute the itemized cost only when the drag endpoints
                // changed; the cost depends on tech_level, which keeps
                // growing while the simulation runs, so recomputing every
                // frame made the total flicker with a still cursor. The
                // tile highlight below must still be drawn every frame.
                const bool costStale = previewCacheStart != startRoad
                  || previewCacheEnd != tileUnderMouse;
                if(costStale) {
                    previewCacheStart = startRoad;
                    previewCacheEnd = tileUnderMouse;
                    previewLandCost = 0;
                    previewBridgeCost = 0;
                    previewLandTiles = 0;
                    previewBridgeTiles = 0;
                    previewTiles = 0;
                }

                int* v1 = ctrDrag ? &currentTile.y :&currentTile.x;
                int* v2 = ctrDrag ? &currentTile.x :&currentTile.y;
                int* l1 = ctrDrag ? &tileUnderMouse.y :&tileUnderMouse.x;
                int* l2 = ctrDrag ? &tileUnderMouse.x :&tileUnderMouse.y;
                int* s1 = ctrDrag ? &stepy: &stepx;
                int* s2 = ctrDrag ? &stepx: &stepy;

                // Itemize the preview: tiles over water become bridges
                // and cost their real (500x) price (FEAT-01, BUG-04).
                while( *v1 != *l1)
                {
                    markTile( painter, currentTile );
                    if(costStale) {
                        if(previewGroup && isTransportGroup(previewGroup)
                          && getWorld().map(currentTile)->is_water())
                        {
                            ConstructionGroup& g = transport_group_at(*previewGroup,
                              *getWorld().map(currentTile));
                            previewBridgeCost += g.getCosts(getWorld());
                            previewBridgeTiles++;
                        }
                        else {
                            previewLandCost += buildCost( currentTile );
                            previewLandTiles++;
                        }
                        previewTiles++;
                    }
                    *v1 += *s1;
                }
                while( *v2 != *l2 + *s2 )
                {
                    markTile( painter, currentTile );
                    if(costStale) {
                        if(previewGroup && isTransportGroup(previewGroup)
                          && getWorld().map(currentTile)->is_water())
                        {
                            ConstructionGroup& g = transport_group_at(*previewGroup,
                              *getWorld().map(currentTile));
                            previewBridgeCost += g.getCosts(getWorld());
                            previewBridgeTiles++;
                        }
                        else {
                            previewLandCost += buildCost( currentTile );
                            previewLandTiles++;
                        }
                        previewTiles++;
                    }
                    *v2 += *s2;
                }
                // Assign to the outer variables used by the floating text
                // below: shadowing locals here left them stuck at 0 and hid
                // the bridge itemization (BUG-09).
                landCost = previewLandCost;
                bridgeCost = previewBridgeCost;
                landTiles = previewLandTiles;
                bridgeTiles = previewBridgeTiles;
                tiles = previewTiles;
                cost = (int)(landCost + bridgeCost);
            }

            // floating itemized cost near the cursor (FEAT-01)
            Vector2 previewPos = getScreenPoint(tileUnderMouse);
            previewPos.x += tileWidth * 0.5f;
            previewPos.y -= 14;
            if(previewBuild) {
                if(bridgeTiles > 0)
                    setFloatingText(
                        std::string(_(previewGroup->name)) + " "
                        + std::to_string(landTiles) + "×"
                        + format_money(landCost) + "$ + " + _("Bridge") + " "
                        + std::to_string(bridgeTiles) + "×"
                        + format_money(bridgeCost) + "$ = "
                        + format_money(cost) + "$",
                      previewPos, 0);
                else
                    setFloatingText(
                        std::string(_(previewGroup->name)) + " "
                        + std::to_string(tiles) + "×"
                        + format_money(cost) + "$", previewPos, 0);
            }
            else {
                setFloatingText(std::string(_("Bulldoze")) + " "
                    + format_money(cost) + "$", previewPos, 0);
            }
        }
        else
        {
            markTile( painter, tileUnderMouse );
            tiles++;
            if( (previewBulldoze ) && realTile( currentTile ) != lastRazed ) {
                    cost += bulldozeCost( tileUnderMouse );
            } else {
                cost += buildCost( tileUnderMouse );
            }
            if(floatingPersistent)
              clearFloatingText();
        }
        std::stringstream prize;
        if( previewBulldoze ){
            if( roadDragging ){
                prize << _("Estimated Bulldoze Cost: ");
            } else {
                prize << _("Bulldoze Cost: ");
            }
            if( cost > 0 ) {
                prize << cost << _("$");
            } else {
                prize << _("n/a");
            }
            printStatusMessage( prize.str() );
        }
        else if( previewBuild)
        {
            std::string buildingName =  previewGroup->name;
            prize << _(buildingName);
            prize << _(": Cost to build ");
            if( cost > 0 ) {
                prize << cost << _("$");
        } else {
                prize << _("n/a");
            }
            printStatusMessage( prize.str() );
        } else {
           showToolInfo( tiles );
        }
    }
    else if(floatingPersistent) {
        clearFloatingText();
    }

    drawFloatingText(painter);
}

/*
 * Show informatiosn about selected Tool
 */
void GameView::showToolInfo( int number /*= 0*/ )
{
    std::stringstream infotextstream;

    if( getUserOperation()->action == UserOperation::ACTION_QUERY ) //query
    {
        infotextstream << _("Query Tool: Show information about selected building.");
    }
    else if( getUserOperation()->action == UserOperation::ACTION_BULLDOZE ) //bulldoze
    {
        infotextstream << _("Bulldozer: remove building -price varies-");
    }
    else if( getUserOperation()->action == UserOperation::ACTION_BUILD )
    {
        infotextstream << getUserOperation()->constructionGroup->getName();
        infotextstream << _(": Cost to build ") << getUserOperation()->constructionGroup->getCosts(getWorld()) <<_("$");
        infotextstream << _(", to bulldoze ") << getUserOperation()->constructionGroup->bul_cost <<_("$") << ".";
        if( number > 1 ){
            infotextstream << _(" To build ") << number << _(" of them ");
            infotextstream << _("will cost about ") << number*getUserOperation()->constructionGroup->getCosts(getWorld()) << _("$") << "-";
        }
    }
    else if ( getUserOperation()->action == UserOperation::ACTION_EVACUATE )
    {
        infotextstream << _("Evacuation of commodities is for free.");
    }
    else if ( getUserOperation()->action == UserOperation::ACTION_FLOOD )
    {
        infotextstream <<  _("Water") << _(": Cost to build ") << GROUP_WATER_COST << _("$");
        infotextstream << _(", to bulldoze ") << GROUP_WATER_BUL_COST << _("$") << ".";
    }

    printStatusMessage( infotextstream.str() );
}

/*
 * Print a Message to the StatusBar.
 */
// TODO: this method should be moved to the Game class
void GameView::printStatusMessage( std::string message ){
    if( message == lastStatusMessage ){
        return;
    }
    Component* root = this;
    while( root->getParent() )
        root = root->getParent();
    Desktop* desktop = dynamic_cast<Desktop*> (root);
    if(!desktop) {
        std::cerr << "Root not a desktop!?!\n";
        return;
    }

    try {
        Paragraph* statusParagraph = getParagraph( *root, "statusParagraph");
        statusParagraph->setText( message );
    } catch(std::exception& e) {
        std::cerr << "Couldn't print status message '" << message  << "': "
            << e.what() << "\n";
        return;
    }
    lastStatusMessage = message;
}

static bool isTransportGroup(const ConstructionGroup* group) {
  if(!group)
    return false;
  switch(group->group) {
    case GROUP_ROAD:
    case GROUP_TRACK:
    case GROUP_RAIL:
    case GROUP_ROAD_BRIDGE:
    case GROUP_TRACK_BRIDGE:
    case GROUP_RAIL_BRIDGE:
      return true;
    default:
      return false;
  }
}

void GameView::setFloatingText(const std::string& text, Vector2 pos,
    Uint32 lifetimeMs) {
  floatingParagraph->setText(text, floatingTextStyle);
  // Re-measure at the text's natural size: Paragraph::setText reflows into
  // the paragraph's *current* width, which Document::resize grows by the
  // padding every frame, so the box used to expand without bound.
  floatingParagraph->resize(-1, -1);
  floatingDoc->resize(floatingParagraph->getWidth() + 8,
    floatingParagraph->getHeight() + 4);
  floatingPos = pos;
  floatingUntil = lifetimeMs ? SDL_GetTicks() + lifetimeMs : 0;
  floatingPersistent = (lifetimeMs == 0);
  floatingActive = true;
}

void GameView::clearFloatingText() {
  floatingActive = false;
  floatingPersistent = false;
}

void GameView::drawFloatingText(Painter& painter) {
  if(!floatingActive)
    return;
  if(floatingUntil && SDL_GetTicks() >= floatingUntil) {
    floatingActive = false;
    return;
  }
  painter.pushTransform();
  painter.translate(floatingPos);
  floatingDoc->draw(painter);
  painter.popTransform();
}

int GameView::bulldozeCost( MapPoint tile ){

    if (!getWorld().map.is_visible(tile))
    {   return 0;}
    Construction *reportingConstruction = getWorld().map(tile)->reportingConstruction;
    if (reportingConstruction)
    {   return reportingConstruction->constructionGroup->bul_cost;}
    else
    {
        int group = getWorld().map(tile)->getGroup();
        if (group == GROUP_DESERT)
        {   return 0;}
        else if (group == GROUP_WATER)
        {   return GROUP_WATER_BUL_COST;}
        else
        {   return 1;}
    }
    return 0;
}

int GameView::buildCost(MapPoint tile) {
  Message::ptr tmp;
  if(!getUserOperation()->isAllowedHere(getWorld(), tile, tmp)
    || getUserOperation()->action == UserOperation::ACTION_QUERY
    || getUserOperation()->action == UserOperation::ACTION_EVACUATE
  )
    return 0;
  if(getUserOperation()->action == UserOperation::ACTION_BUILD) {
    if (getWorld().map(tile)->is_water()) //building a bridge
      return BRIDGE_FACTOR * getUserOperation()->constructionGroup->getCosts(getWorld());
    else //building on land
      return getUserOperation()->constructionGroup->getCosts(getWorld());
  }
  if(getUserOperation()->action == UserOperation::ACTION_FLOOD)
    return GROUP_WATER_COST;
  return 0;
}

//Register as Component
IMPLEMENT_COMPONENT_FACTORY(GameView)

/** @file lincity-ng/GameView.cpp */
