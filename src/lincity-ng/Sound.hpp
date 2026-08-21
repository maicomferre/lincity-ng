/* ---------------------------------------------------------------------- *
 * src/lincity-ng/Sound.hpp
 * This file is part of Lincity-NG.
 *
 * Copyright (C) 2005      Wolfgang Becker <uafr@gmx.de>
 * Copyright (C) 2025      David Bears <dbear4q@gmail.com>
 * Copyright (C) 2026 Marc Young <myoung008@gmail.com>
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

#ifndef __SOUND_HPP__
#define __SOUND_HPP__

#include <SDL3/SDL.h>    // for SDL_Thread
#include <SDL3_mixer/SDL_mixer.h>   // for MIX_Audio
#include <atomic>        // for atomic_bool
#include <filesystem>
#include <map>           // for multimap
#include <string>        // for basic_string, string, operator<
#include <vector>        // for vector

class MapTile;

//TODO make use for these:
enum MusicState {
    PLAY, PAUSE, STOP
};

//Used for changing tracks.
enum MusicTransport {
    NEXT_TRACK, PREV_TRACK, NEXT_OR_FIRST_TRACK
};

struct song {
    std::string title;
    std::filesystem::path filename;
    int trackNumber;
    float lowestTechLevel;
    float highestTechLevel;
};

class Sound
{
public:
    Sound();
    ~Sound();

    void playSound(const std::string& name);
    void playSound(const MapTile& tile);
    void playASound(MIX_Audio *chunk);
    void playMusic();
    static void musicHalted(void *userdate, MIX_Track *track);
    void changeTrack(MusicTransport command);
    /* Audio callbacks only set this request. Track changes and GUI updates
     * are consumed by the main thread in Game/MainMenu. */
    bool consumeTrackChangeRequest();
    void enableMusic(bool enabled);
    /** set Music volume 0..100, 0=silent */
    void setMusicVolume(int vol);
    /** set Sound effect volume (0..100) */
    void setSoundVolume(int vol);
    /** Load song data from theme directory */
    void loadMusicTheme();
    /** information about currently playing track. Title, filename, track number, and tech levels allowed to play it. */
    song currentTrack;

    void setTechLevel(int tech) { tech_level = tech; }


private:
    static int soundThread(void* ptr);
    void loadWaves();
    std::string getIdName(const std::string& filename);

    typedef std::multimap<std::string,MIX_Audio*> chunks_t;
    chunks_t waves;
    bool audioOpen;
    SDL_Thread* loaderThread;
    MIX_Audio* currentMusic;
    int totalTracks;
    std::vector<song> playlist;

    int tech_level = 0;

    MIX_Track *musicTrack;
    MIX_Track *soundTrack;
    MIX_Mixer *mixer;
    std::atomic_bool trackChangeRequested{false};
};

//TODO: singleton
Sound* getSound();

#endif

/** @file lincity-ng/Sound.hpp */
