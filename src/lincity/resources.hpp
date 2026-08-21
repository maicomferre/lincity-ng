/* ---------------------------------------------------------------------- *
 * src/lincity/resources.hpp
 * This file is part of Lincity-NG.
 *
 * Copyright (C) 2022-2024 David Bears <dbear4q@gmail.com>
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

#ifndef __LINCITY_RESOURCES_HPP__
#define __LINCITY_RESOURCES_HPP__

#include <iostream>
#include <map>
#include <SDL3_mixer/SDL_mixer.h>  // for MIX_Audio
#include <SDL3/SDL.h>       // for SDL_Surface
#include <string>           // for string
#include <vector>           // for vector

#include "gui/Texture.hpp"  // for Texture

// Tint applied to a freshly loaded image (e.g. a colored vehicle variant).
struct Tint {
  Uint8 r, g, b;
  bool enabled;
  Tint() : r(255), g(255), b(255), enabled(false) {}
};

class GraphicsInfo
{
    public:
    GraphicsInfo(void){
        texture = (Texture*)'\0';
        image = (SDL_Surface*)'\0';
        x = 0;
        y = 0;
    }

    Texture* texture;
    SDL_Surface* image;
    int x, y;
};

//all instances are added to resMap
class ResourceGroup {
public:

    ResourceGroup(const std::string &tag)
    {
        graphicsInfoVector.clear();
        chunks.clear();
        resourceID = tag;
        images_loaded = false;
        sounds_loaded = false;
        is_vehicle = false;
        //std::cout << "new resourceGroup: " << tag << std::endl;
        if (resMap.count(tag))
        {   std::cout << "rejecting " << tag << " as another ResourceGroup"<< std::endl;}
        else
        {   resMap[tag] = this;}
    }
    ~ResourceGroup()
    {
        resetGraphics();
        if ( resMap.count(resourceID))
        {
            resMap.erase(resourceID);
            //std::cout << "sayonara: " << resourceID << std::endl;
        }
        else if(!resMap.empty())
        {   std::cout << "error: unreachable resourceGroup: " << resourceID << std::endl;}
    }
    // Release per-Game graphics while keeping the module-owned resource
    // groups registered for the next Game instance (BUG-21/R6).
    void resetGraphics()
    {
        for(auto& gfx : graphicsInfoVector) {
            if(gfx.image) {
                SDL_DestroySurface(gfx.image);
                gfx.image = 0;
            }
            // GameView creates these textures directly; they are not owned
            // by TextureManager's load cache.
            delete gfx.texture;
            gfx.texture = 0;
        }
        graphicsInfoVector.clear();
        images_loaded = false;
    }
    std::string resourceID;
    bool images_loaded;
    bool sounds_loaded;
    bool is_vehicle; //vehicles are always rendered on upper left tile
    Tint tint;       //optional color tint applied when the image is loaded
    std::vector<MIX_Audio *> chunks;
    std::vector<GraphicsInfo> graphicsInfoVector;
    void growGraphicsInfoVector(void)
    {   graphicsInfoVector.resize(graphicsInfoVector.size() + 1);}
    static std::map<std::string, ResourceGroup*> resMap;
};

struct ExtraFrame{
    ExtraFrame(void){
        move_x = 0;
        move_y = 0;
        frame = 0;
        resourceGroup = 0;
    }

    int move_x; // >0 moves frame to the right
    int move_y; // >0 moves frame downwards
    int frame; //frame >= 0 will be rendered as overlay
    ResourceGroup *resourceGroup; //overlay frame is choosen from its GraphicsInfoVector
};

#endif  // __LINCITY_RESOURCES_HPP__
