/* ---------------------------------------------------------------------- *
 * src/lincity-ng/MapThumbnail.cpp
 * This file is part of Lincity-NG.
 *
 * Copyright (C) 2026      Maicom Ferreira <maicomferre3242@gmail.com>
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

#include "MapThumbnail.hpp"

#include <SDL3/SDL.h>

#include <libxml++/parsers/textreader.h>

#include "MiniMap.hpp"                  // for minimapColorNormal
#include "gui/ComponentFactory.hpp"       // for IMPLEMENT_COMPONENT_FACTORY
#include "gui/Color.hpp"                // for Color
#include "gui/Painter.hpp"              // for Painter
#include "gui/Rect2D.hpp"               // for Rect2D
#include "gui/Texture.hpp"              // for Texture
#include "gui/TextureManager.hpp"       // for texture_manager
#include "lincity/MapPoint.hpp"         // for MapPoint
#include "lincity/world.hpp"            // for World, Map
#include "util/xmlutil.hpp"             // for unexpectedXmlAttribute, ...

IMPLEMENT_COMPONENT_FACTORY(MapThumbnail)

MapThumbnail::MapThumbnail()
{
}

void
MapThumbnail::parse(xmlpp::TextReader& reader) {
  while(reader.move_to_next_attribute()) {
    xmlpp::ustring aname = reader.get_name();
    xmlpp::ustring value = reader.get_value();
    if(parseAttribute(reader));
    else if(aname == "width")
      width = xmlParse<float>(value);
    else if(aname == "height")
      height = xmlParse<float>(value);
    else
      unexpectedXmlAttribute(reader);
  }
  reader.move_to_element();

  // no child elements
  if(!reader.is_empty_element() && reader.read())
  while(reader.get_node_type() != xmlpp::TextReader::NodeType::EndElement) {
    if(reader.get_node_type() != xmlpp::TextReader::NodeType::Element) {
      reader.next();
      continue;
    }
    unexpectedXmlElement(reader);
    reader.next();
  }
}

void
MapThumbnail::setWorld(const World* world) {
  texture.reset();
  if(!world || !texture_manager)
    return;

  const int len = world->map.len();
  SDL_Surface* surface = SDL_CreateSurface(len, len,
    SDL_PIXELFORMAT_RGBA8888);
  if(!surface)
    return;
  if(!SDL_LockSurface(surface)) {
    SDL_DestroySurface(surface);
    return;
  }

  Uint8* pixels = static_cast<Uint8*>(surface->pixels);
  for(int y = 0; y < len; y++)
    for(int x = 0; x < len; x++) {
      const Color color = minimapColorNormal(*world->map(MapPoint(x, y)));
      Uint8* pixel = pixels + y * surface->pitch + x * 4;
      pixel[0] = color.r;
      pixel[1] = color.g;
      pixel[2] = color.b;
      pixel[3] = color.a;
    }
  SDL_UnlockSurface(surface);

  texture.reset(texture_manager->create(surface));
  SDL_DestroySurface(surface);
}

void
MapThumbnail::draw(Painter& painter) {
  if(!texture)
    return;
  painter.drawStretchTexture(texture.get(),
    Rect2D(0, 0, getWidth(), getHeight()));
}

/** @file lincity-ng/MapThumbnail.cpp */
