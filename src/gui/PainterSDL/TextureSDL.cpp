/* ---------------------------------------------------------------------- *
 * src/gui/PainterSDL/TextureSDL.cpp
 * This file is part of Lincity-NG.
 *
 * Copyright (C) 2005      Matthias Braun <matze@braunis.de>
 * Copyright (C) 2025      David Bears <dbear4q@gmail.com>
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

#include "TextureSDL.hpp"

#include <SDL3/SDL.h>
#include <cassert>
#include <iostream>

TextureSDL::TextureSDL(SDL_Texture *tx) : tx(tx) {
  assert(tx);
  width = tx->w;
  height = tx->h;
  assert(width && height);
}

TextureSDL::~TextureSDL() {
  for(SDL_Texture *mip : mipmaps)
    SDL_DestroyTexture(mip);
  SDL_DestroyTexture(tx);
}

void
TextureSDL::addMipmap(SDL_Texture *mipmap) {
  assert(mipmap);
  mipmaps.push_back(mipmap);
}

SDL_Texture*
TextureSDL::selectMipmap(int destWidth, int destHeight) const {
  // mipmaps are sorted from largest to smallest; pick the largest one that
  // still covers the destination so we never upscale a smaller mipmap.
  SDL_Texture *selected = tx;
  for(SDL_Texture *mip : mipmaps) {
    if(mip->w >= destWidth && mip->h >= destHeight)
      selected = mip;
    else
      break;
  }
  return selected;
}

void
TextureSDL::setScaleMode(ScaleMode mode) {
  SDL_ScaleMode sdlMode;
  switch(mode) {
  case ScaleMode::NEAREST:
    sdlMode = SDL_SCALEMODE_NEAREST;
    break;
  case ScaleMode::LINEAR:
    sdlMode = SDL_SCALEMODE_LINEAR;
    break;
  case ScaleMode::ANISOTROPIC:
    sdlMode = SDL_SCALEMODE_LINEAR; // re-use Linear for SDL_ScaleModeBest, per sdl3 migration docs
    break;
  default:
    std::cerr << "warning: scale mode not supported" << std::endl;
    assert(false);
    return;
  }

  if(!SDL_SetTextureScaleMode(tx, sdlMode)) {
    std::cerr << "warning: failed to set scale mode" << std::endl;
    assert(false);
    return;
  }
  for(SDL_Texture *mip : mipmaps) {
    if(!SDL_SetTextureScaleMode(mip, sdlMode)) {
      std::cerr << "warning: failed to set scale mode on mipmap" << std::endl;
      assert(false);
      return;
    }
  }
}

/** @file gui/PainterSDL/TextureSDL.cpp */
