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

#ifndef MISTER_PORT_H
#define MISTER_PORT_H

// MiSTer hybrid core: ScummVM runs on the ARM, the FPGA core shows the
// picture at 15kHz and provides audio, the FM chip, the MIDI synthesizer and
// input. SDL's "mister" drivers (hybrid/sdl2) do most of the work; this is
// what is specific to ScummVM, from hybrid/template/game/mister_port.c.
// Everything is a no-op without the core, so the PC build still runs.

// Joystick buttons as SDL sees them: the buttons of the core's "J1," list
// (core/ScummVM.sv) in order, then the two the SDL driver adds
enum {
	MISTER_JOY_LEFT_CLICK,
	MISTER_JOY_RIGHT_CLICK,
	MISTER_JOY_SKIP,
	MISTER_JOY_SKIP_LINE,
	MISTER_JOY_SLOW_POINTER,
	MISTER_JOY_GAME_MENU,
	MISTER_JOY_KEYBOARD,
	MISTER_JOY_MAIN_MENU,
	MISTER_JOY_CORE_OK,
	MISTER_JOY_CORE_BACK,

	MISTER_JOY_ACTIONS = 8,
	MISTER_JOY_MENU_OK = 28,
	MISTER_JOY_MENU_BACK = 29
};

// The start every port has (hybrid/CLAUDE.md, "every port starts the same
// way"): the core's intro, then "Loading ScummVM..." and the logo. Returns
// false when the core is gone meanwhile
bool MiSTer_Start();
// Exit code of the process after the player quit (danik_hybrid_launch.sh)
int MiSTer_ExitCode();

// OSD options of the game (CONF_STR in core/ScummVM.sv, status bits 24..63)
enum { MISTER_POINTER_NORMAL, MISTER_POINTER_SLOW, MISTER_POINTER_FAST };
int MiSTer_PointerSpeed();
// 0..256, as ScummVM's volumes
int MiSTer_SoundVolume();
int MiSTer_MusicVolume();
int MiSTer_SpeechVolume();
enum { MISTER_SUBTITLES_GAME, MISTER_SUBTITLES_SPEECH, MISTER_SUBTITLES_TEXT, MISTER_SUBTITLES_BOTH };
int MiSTer_Subtitles();
// ScummVM's launcher in 640x480 (true) or 320x200
bool MiSTer_LauncherHires();
// Cheats: SCUMM's fast mode, 0 off, 1 fast (Ctrl+F), 2 turbo (Ctrl+G)
int MiSTer_FastMode();
// Music: MIDI (true) or the FM chip, and the MT32-pi's synth when it plays it
bool MiSTer_MusicMidi();
bool MiSTer_PiMT32();

// The Cheats page's line is greyed out while no SCUMM game runs (menu mask
// bit 0, "D0")
void MiSTer_SetScummRunning(bool running);

// Development (mister-prof.cpp): MISTER_PROF=<file> samples where the time
// goes, and the frames shown per second go to the log
void MiSTer_ProfStart();
void MiSTer_ProfFrame();

#endif
