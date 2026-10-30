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

#ifndef BACKENDS_EVENTS_MISTER_H
#define BACKENDS_EVENTS_MISTER_H

#include "backends/events/sdl/sdl-events.h"

class OSystem_SDL_MiSTer;

// The MiSTer's input as SDL's "mister" driver delivers it: keyboard and mouse
// as they are; the core's joystick 1 a game controller of ScummVM's (A, B, X,
// Y, L, R, Select and Start the actions of the core's J1 list, the d-pad,
// the sticks), so that the keymapper maps it; and Menu OK / Menu Back
// (hybrid/CLAUDE.md, "menus work with Menu OK and Menu Back"), done here: in
// ScummVM's dialogs Esc, and Enter or, on a button that is one of the pad's
// actions as well, the click at the pointer, instead of that action; in a game
// a left click and Esc, and nothing on such a button, which does its action.
class MisterEventSource : public SdlEventSource {
public:
	MisterEventSource(OSystem_SDL_MiSTer *system);
	~MisterEventSource() override;

	// The SDL game controller mapping of the core's joystick 1, before
	// ScummVM opens it
	static void addControllerMapping();

	bool pollEvent(Common::Event &event) override;

protected:
	bool dispatchSDLEvent(SDL_Event &ev, Common::Event &event) override;

private:
	enum Sent { kSentNothing, kSentReturn, kSentEscape, kSentLeftClick };
	enum { kOK, kBack };

	bool menuButton(int which, bool down, bool sameAsAction, Common::Event &event);
	bool actionHeld() const;
	uint32 menuActions(int which) const;
	bool isStickDpad(const SDL_ControllerButtonEvent &button);
	bool isMenuAction(const SDL_ControllerButtonEvent &button);

	OSystem_SDL_MiSTer *_system;
	SDL_Joystick *_pad;
	int _held[2];   // the buttons that are Menu OK (Menu Back) held, as a mask
	Sent _sent[2];  // what its press sent, for its release
	uint32 _stickDpad; // the d-pad directions held that are the stick's (MH_JOY_*)
	uint32 _menuAction; // the pad's actions held that are Menu OK / Back (MH_JOY_BUTTON)
};

#endif
