/* ---------------------------------------------------------------------- *
 * src/lincity-ng/Video.cpp
 * This file is part of Lincity-NG.
 *
 * Copyright (C) 2005      Matthias Braun <matze@braunis.de>
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

#include "main.hpp"

#include <SDL3/SDL.h>                            // for SDL_GetError, SDL_Se...
#include <iostream>                              // for basic_ostream, opera...

#include "Config.hpp"                            // for getConfig, Config
#include "config.h"                              // for PACKAGE_NAME, PACKAGE_VERSION
#include "gui/FontManager.hpp"                   // for FontManager, fontManager
#include "gui/Painter.hpp"                       // for Painter
#include "gui/PainterSDL/PainterSDL.hpp"         // for PainterSDL
#include "gui/PainterSDL/TextureManagerSDL.hpp"  // for TextureManagerSDL
#include "gui/TextureManager.hpp"                // for texture_manager

#ifndef DISABLE_GL_MODE
#include <SDL3/SDL_opengl.h>                          // for glDisable, glLoadIde...

#include "gui/PainterGL/PainterGL.hpp"           // for PainterGL
#include "gui/PainterGL/TextureManagerGL.hpp"    // for TextureManagerGL
#endif

SDL_Window* window = NULL;
SDL_GLContext window_context = NULL;
SDL_Renderer* window_renderer = NULL;
Painter* painter = 0;

void videoSizeChanged(int width, int height) {
#ifndef DISABLE_GL_MODE
    if(getConfig()->useOpenGL.get()) {
        /* Reset OpenGL state */
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        glClearColor(0, 0, 0, 0);
        glViewport(0, 0, width, height);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, width, height, 0, -1, 1);

        glClear(GL_COLOR_BUFFER_BIT);
    }
#endif
}

void resizeVideo(int width, int height, bool fullscreen)
{
    // Set fullscreen (video mode change)
    SDL_SetWindowFullscreen(window, fullscreen);
    if (!fullscreen) {
        SDL_SetWindowSize(window, width, height);
    }
}

void initVideo(int width, int height)
{
    // BUG-10: if initVideo is called again (e.g. the headless test suite
    // calls it once per test that needs a GUI), the previous painter/
    // fontManager/texture_manager/window would be leaked. Clean them up
    // first. In the real game initVideo is called once, so this is inert.
    if(painter)          { delete painter;          painter = nullptr; }
    if(fontManager)      { delete fontManager;      fontManager = nullptr; }
    if(texture_manager)  { delete texture_manager;  texture_manager = nullptr; }
    if(window_renderer)  { SDL_DestroyRenderer(window_renderer); window_renderer = nullptr; }
#ifndef DISABLE_GL_MODE
    if(window_context)   { SDL_GL_DestroyContext(window_context); window_context = nullptr; }
#endif
    if(window)           { SDL_DestroyWindow(window); window = nullptr; }

    Uint32 flags = 0;

    flags = SDL_WINDOW_RESIZABLE;
#ifndef DISABLE_GL_MODE
    if(getConfig()->useOpenGL.get()) {
        flags |= SDL_WINDOW_OPENGL;
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 1);
        SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 1);
        SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 1);
        //SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
        //SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    }
#endif
    if(getConfig()->useFullScreen.get())
        flags |= SDL_WINDOW_FULLSCREEN;

    window = SDL_CreateWindow(PACKAGE_NAME " " PACKAGE_VERSION,
                              width, height,
                              flags);

    if(getConfig()->useFullScreen.get()) {
      // actual window size may be different than requested
      SDL_GetWindowSize(window, &width, &height);
      getConfig()->videoX.session = width;
      getConfig()->videoY.session = height;
    }

#ifndef DISABLE_GL_MODE
    if(getConfig()->useOpenGL.get()) {
        window_context = SDL_GL_CreateContext(window);
        SDL_GL_SetSwapInterval(1);

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        glClearColor(0, 0, 0, 0);
        glViewport(0, 0, width, height);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, width, height, 0, -1, 1);

        glClear(GL_COLOR_BUFFER_BIT);

        painter = new PainterGL(window);
        std::cout << "\nOpenGL Mode " << width;
        std::cout << "x" << height << "\n";

        texture_manager = new TextureManagerGL();
    }
    else
#endif
    {
        window_renderer = SDL_CreateRenderer(window, nullptr);

        painter = new PainterSDL(window_renderer);
        std::cout << "\nSDL Mode " << width;
        std::cout << "x"<< height <<"\n";

        texture_manager = new TextureManagerSDL(window_renderer);
    }

    fontManager = new FontManager();
}


/** @file lincity-ng/Video.cpp */
