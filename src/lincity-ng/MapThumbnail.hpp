/* ---------------------------------------------------------------------- *
 * src/lincity-ng/MapThumbnail.hpp
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

/* A small terrain preview of a World, painted with the same colors as the
 * minimap. Used by the scenario information panel in the new-game menu
 * (FEAT-02); when no world is set nothing is drawn. */

#ifndef LINCITYNG_MAPTHUMBNAIL_HPP
#define LINCITYNG_MAPTHUMBNAIL_HPP

#include <memory>

#include "gui/Component.hpp"        // for Component

class Painter;
class Texture;
class World;
namespace xmlpp {
class TextReader;
}  // namespace xmlpp

class MapThumbnail : public Component {
public:
    MapThumbnail();

    void parse(xmlpp::TextReader& reader);

    void draw(Painter& painter) override;

    /* Generate (or clear) the preview texture from the world. */
    void setWorld(const World* world);

private:
    std::unique_ptr<Texture> texture;
};

#endif // LINCITYNG_MAPTHUMBNAIL_HPP
