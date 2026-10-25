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

// MiSTer hybrid core support, see mister_port.h

#define FORBIDDEN_SYMBOL_ALLOW_ALL

#include "backends/platform/sdl/mister/mister_port.h"

#include <mister_hybrid.h>

#define EXIT_TO_MENU 0
#define EXIT_RELOADED 43 // danik_hybrid_launch.sh starts the game again, once the core has settled

// The one game of package/games.txt: ScummVM itself, whose launcher has the
// player's games. It has no files to check (an empty launcher is where a
// player starts): NULL, always there.
static const mh_game Games[] = {
	{ "ScummVM", "ScummVM", nullptr, nullptr, nullptr }
};

bool MiSTer_Start() {
	int listed = 0;

	// the lines under the MiSTer logo, then the logo and "Loading ScummVM..."
	// (MH_GameLogo(), host version 17)
	if (!MH_Open())
		return true; // no core: the PC build
	return MH_PickGame(Games, 1, nullptr, nullptr, &listed) >= 0;
}

int MiSTer_ExitCode() {
	// The player loaded the core again while ScummVM ran
	if (MH_CoreReloaded())
		return EXIT_RELOADED;
	return EXIT_TO_MENU;
}

// OSD options of the game (CONF_STR in core/ScummVM.sv, status bits 24..63).
// The first entry of each is 0, the default.
//   "O[25:24],Pointer Speed,Normal,Slow,Fast;"
int MiSTer_PointerSpeed() { return MH_OSD_GAME_BITS(MH_OSDStatus(), 24, 2); }

// The volumes' entries 100%, 90%, ... 0%: 0..10
static int volume(int entry) {
	return entry >= 10 ? 0 : 256 * (10 - entry) / 10;
}

int MiSTer_SoundVolume() { return volume(MH_OSD_SOUND_VOLUME(MH_OSDStatus())); }
int MiSTer_MusicVolume() { return volume(MH_OSD_MUSIC_VOLUME(MH_OSDStatus())); }
//   "O[29:26],Speech Volume,100%,90%,...,0%;"
int MiSTer_SpeechVolume() { return volume(MH_OSD_GAME_BITS(MH_OSDStatus(), 26, 4)); }
//   "O[31:30],Subtitles,Game Default,Speech Only,Subtitles Only,Both;"
int MiSTer_Subtitles() { return MH_OSD_GAME_BITS(MH_OSDStatus(), 30, 2); }
//   "O[34],Launcher,320x200,640x480;"
bool MiSTer_LauncherHires() { return MH_OSD_GAME_BITS(MH_OSDStatus(), 34, 1); }
//   "P2O[33:32],Fast Mode (SCUMM),Off,Fast,Turbo;"
int MiSTer_FastMode() {
	int mode = MH_OSD_GAME_BITS(MH_OSDStatus(), 32, 2);
	return mode > 2 ? 2 : mode;
}

// The framework's lines (hybrid/README.md, "MIDI and the MT32-pi"): Music
// (bit 87, MIDI / FM chip), MT32-pi (bit 96, On / Off), MT32-pi Synth (bit
// 97, SoundFont / MT-32)
bool MiSTer_MusicMidi() {
	return !((MH_OSDStatusHigh() >> (87 - 64)) & 1);
}

bool MiSTer_PiMT32() {
	const uint64_t high = MH_OSDStatusHigh();

	return MH_MidiPiConnected() && !((high >> (96 - 64)) & 1) && ((high >> (97 - 64)) & 1);
}

void MiSTer_SetScummRunning(bool running) {
	MH_SetMenuMask(running ? 0 : 1);
}
