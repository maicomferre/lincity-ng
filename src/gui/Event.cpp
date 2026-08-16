/*
Copyright (C) 2005 Matthias Braun <matze@braunis.de>
Copyright (C) 2024 David Bears <dbear4q@gmail.com>
Copyright (C) 2026 Marc Young <myoung008@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include "Event.hpp"

#include <SDL3/SDL.h>// for SDL_Event, SDL_KEYDOWN, SDL_KEYUP, SDL_MOUSEBUTT...
#include <assert.h>  // for assert
#include <cstring>   // for memcpy

namespace {
/* SDL delivers some synthesized events whose scancode field is not a valid
 * SDL_Scancode; reading the enum-typed field directly is UB in that case.
 * Copy the representation bytes instead and map anything outside the
 * scancode table to UNKNOWN (the switch statements just won't match, same
 * as before). */
SDL_Scancode sanitizeScancode(const SDL_Scancode& field) {
  unsigned int raw = 0;
  std::memcpy(&raw, &field, sizeof(field));
  return (raw < SDL_SCANCODE_COUNT)
    ? static_cast<SDL_Scancode>(raw) : SDL_SCANCODE_UNKNOWN;
}
} // namespace

Event::Event(SDL_Event& event)
    : inside(true)
{
    switch(event.type) {
        case SDL_EVENT_KEY_UP:
            type = KEYUP;
            key = event.key.key;
            mod = event.key.mod;
            scancode = sanitizeScancode(event.key.scancode);
            break;
        case SDL_EVENT_KEY_DOWN:
            type = KEYDOWN;
            key = event.key.key;
            mod = event.key.mod;
            scancode = sanitizeScancode(event.key.scancode);
            break;
        case SDL_EVENT_MOUSE_MOTION:
            type = MOUSEMOTION;
            mousepos = Vector2(event.motion.x, event.motion.y);
            mousemove = Vector2(event.motion.xrel, event.motion.yrel);
            mousebuttonstate = event.motion.state;
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            type = MOUSEBUTTONUP;
            mousepos = Vector2(event.button.x, event.button.y);
            mousebutton = event.button.button;
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            type = MOUSEBUTTONDOWN;
            mousepos = Vector2(event.button.x, event.button.y);
            mousebutton = event.button.button;
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            type = MOUSEWHEEL;
            scrolly = event.wheel.integer_y;
            scrolly_precise = event.wheel.y;
            mousepos = Vector2(event.wheel.mouse_x, event.wheel.mouse_y);
            break;
        case SDL_EVENT_WINDOW_MOUSE_ENTER:
            type = WINDOWENTER;
            break;
        case SDL_EVENT_WINDOW_MOUSE_LEAVE:
            type = WINDOWLEAVE;
            break;
        default:
            assert(false);
    }
}

Event::Event(float _elapsedTime)
    : type(UPDATE), inside(false), elapsedTime(_elapsedTime)
{}

Event::Event(Type type)
    : type(type)
{}

/** @file gui/Event.cpp */
