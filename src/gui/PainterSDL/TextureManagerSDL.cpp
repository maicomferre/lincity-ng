/* ---------------------------------------------------------------------- *
 * src/gui/PainterSDL/TextureManagerSDL.cpp
 * This file is part of Lincity-NG.
 *
 * Copyright (C) 2005      Matthias Braun <matze@braunis.de>
 * Copyright (C) 2025      David Bears <dbear4q@gmail.com>
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

#include "TextureManagerSDL.hpp"

#include <SDL3/SDL.h>      // for SDL_Texture, SDL_CreateTextureFromSurface
#include <stdexcept>

#include "TextureSDL.hpp"  // for TextureSDL

/* ---------------------------------------------------------------------- *
 * Mipmap generation.
 *
 * SDL3's renderer does not provide real mipmaps, so when zooming out the
 * renderer has to minify a large texture into a few pixels, which aliases
 * badly. We pre-generate a chain of half-size versions ourselves. To avoid
 * the dark halos/gaps between adjacent tiles that plain bilinear mipmap
 * generation produces, each 2x2 block is averaged with premultiplied alpha,
 * so fully transparent pixels don't darken the result.
 * ---------------------------------------------------------------------- */

static const Uint8 ALPHA_BARRIER = 100;

/* returns a new SDL_Surface (RGBA8888) half the size of src, or nullptr if
 * src is too small to halve. Uses premultiplied-alpha averaging so that
 * transparent halo pixels don't darken tile edges. */
static SDL_Surface*
halveSurface(const SDL_Surface *src) {
  int newW = src->w / 2;
  int newH = src->h / 2;
  if(newW < 1 || newH < 1)
    return nullptr;

  SDL_Surface *dst = SDL_CreateSurface(newW, newH, SDL_PIXELFORMAT_RGBA8888);
  if(!dst)
    throw std::runtime_error(SDL_GetError());

  for(int y = 0; y < newH; ++y) {
    for(int x = 0; x < newW; ++x) {
      int r = 0, g = 0, b = 0, a = 0;
      int count = 0;
      for(int dy = 0; dy < 2; ++dy) {
        for(int dx = 0; dx < 2; ++dx) {
          int sx = x * 2 + dx;
          int sy = y * 2 + dy;
          if(sx >= src->w || sy >= src->h) continue;
          Uint8 pr, pg, pb, pa;
          if(!SDL_ReadSurfacePixel((SDL_Surface*)src, sx, sy,
            &pr, &pg, &pb, &pa))
            continue;
          if(pa < ALPHA_BARRIER)
            continue; // skip near-transparent pixels (tile halo)
          r += pr * pa;
          g += pg * pa;
          b += pb * pa;
          a += pa;
          count++;
        }
      }
      Uint8 outR = 0, outG = 0, outB = 0, outA = 0;
      if(count > 0) {
        outR = (Uint8)(r / a);
        outG = (Uint8)(g / a);
        outB = (Uint8)(b / a);
        outA = (Uint8)(a / count);
      }
      SDL_WriteSurfacePixel(dst, x, y, outR, outG, outB, outA);
    }
  }

  return dst;
}

TextureManagerSDL::TextureManagerSDL(SDL_Renderer *renderer)
  : renderer(renderer)
{}

TextureManagerSDL::~TextureManagerSDL()
{}

Texture *
TextureManagerSDL::create(SDL_Surface *image) {
  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, image);
  if(!texture) {
    throw std::runtime_error(SDL_GetError());
  }
  TextureSDL *result = new TextureSDL(texture);

  // generate mipmaps while both dimensions can still be halved
  SDL_Surface *level = image;
  while(level && level->w >= 2 && level->h >= 2) {
    SDL_Surface *half = halveSurface(level);
    if(!half) break;
    SDL_Texture *mip = SDL_CreateTextureFromSurface(renderer, half);
    if(!mip) {
      SDL_DestroySurface(half);
      break;
    }
    result->addMipmap(mip);
    if(level != image)
      SDL_DestroySurface(level);
    level = half;
  }
  if(level && level != image)
    SDL_DestroySurface(level);

  return result;
}


/** @file gui/PainterSDL/TextureManagerSDL.cpp */
