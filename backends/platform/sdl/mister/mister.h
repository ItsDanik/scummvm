/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef PLATFORM_SDL_MISTER_H
#define PLATFORM_SDL_MISTER_H

#include "backends/platform/sdl/posix/posix.h"

// ScummVM as a MiSTer hybrid core (github.com/ItsDanik/Hybrid_MiSTer): the
// FPGA core shows the picture at 15kHz in the game's own resolution, plays
// the sound, the FM chip's music and the MIDI synthesizer's, and passes the
// MiSTer's keyboard, mouse and pads on. Everything the game writes stays in
// the folder it runs in (games/dnk/ScummVM, on the SD card or a USB drive);
// themes and engine data are next to the program.
class OSystem_SDL_MiSTer : public OSystem_POSIX {
public:
	void initBackend() override;
	bool hasFeature(Feature f) override;

	void engineInit() override;
	void engineDone() override;

	Common::KeymapperDefaultBindings *getKeymapperDefaultBindings() override;

	// The OSD's options, applied as they change: called with every event poll
	void applyOSD(bool force);

protected:
	Common::Path getDefaultConfigFileName() override;
	Common::Path getDefaultLogFileName() override;

private:
	void applyMusicDevice();

	int _lastPointer = -1;
	int _lastSound = -1;
	int _lastMusic = -1;
	int _lastSpeech = -1;
	int _lastSubtitles = -1;
	int _lastLauncher = -1;
};

#endif
