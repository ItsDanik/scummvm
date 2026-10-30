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

#define FORBIDDEN_SYMBOL_ALLOW_ALL

#include "backends/platform/sdl/mister/mister-events.h"
#include "backends/platform/sdl/mister/mister-graphics.h"
#include "backends/platform/sdl/mister/mister.h"
#include "backends/platform/sdl/mister/mister_port.h"

#include "common/events.h"
#include "common/system.h"
#include "common/config-manager.h"
#include "engines/engine.h"
#include "gui/gui-manager.h"

#include <mister_hybrid.h>

void MisterEventSource::addControllerMapping() {
	char guid[33];
	char mapping[512];

	if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) < 0 || SDL_NumJoysticks() < 1)
		return;
	SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(0), guid, sizeof(guid));
	// the buttons in the order of the core's J1 list, the d-pad is the hat,
	// the sticks axes 0..3, L2 / R2 axes 4 and 5, L3 / R3 buttons 30 and 31
	// (hybrid/sdl2/SDL_mistervideo.c)
	snprintf(mapping, sizeof(mapping),
	         "%s,MiSTer Joystick 1,a:b0,b:b1,x:b2,y:b3,leftshoulder:b4,rightshoulder:b5,back:b6,start:b7,"
	         "dpup:h0.1,dpright:h0.2,dpdown:h0.4,dpleft:h0.8,leftx:a0,lefty:a1,rightx:a2,righty:a3,"
	         "lefttrigger:a4,righttrigger:a5,leftstick:b30,rightstick:b31,",
	         guid);
	if (SDL_GameControllerAddMapping(mapping) < 0)
		warning("MiSTer: no game controller mapping for the core's joystick: %s", SDL_GetError());
}

MisterEventSource::MisterEventSource(OSystem_SDL_MiSTer *system) : _system(system), _pad(nullptr), _stickDpad(0), _menuAction(0) {
	// a handle of our own, for its Menu OK / Back buttons and what else
	// is held (ScummVM has it open as a game controller)
	if (SDL_NumJoysticks() > 0)
		_pad = SDL_JoystickOpen(0);
	_held[kOK] = _held[kBack] = 0;
	_sent[kOK] = _sent[kBack] = kSentNothing;
}

MisterEventSource::~MisterEventSource() {
	if (_pad)
		SDL_JoystickClose(_pad);
}

bool MisterEventSource::pollEvent(Common::Event &event) {
	// the OSD's options take effect while the game runs
	_system->applyOSD(false);
	return SdlEventSource::pollEvent(event);
}

bool MisterEventSource::dispatchSDLEvent(SDL_Event &ev, Common::Event &event) {
	// SDL's quit: another core loaded, or this one loaded again (the
	// library's MH_CheckAlive()), or SIGTERM from the launch script. ScummVM
	// leaves at once, whatever Return to the launcher and Confirm exit say:
	// a game would go back to the launcher, or wait for an answer, with no
	// core there (the transient settings come before the player's)
	if (ev.type == SDL_QUIT) {
		ConfMan.setBool("gui_return_to_launcher_at_exit", false, Common::ConfigManager::kTransientDomain);
		ConfMan.setBool("confirm_exit", false, Common::ConfigManager::kTransientDomain);
	}
	if ((ev.type == SDL_JOYBUTTONDOWN || ev.type == SDL_JOYBUTTONUP) && _pad &&
	    ev.jbutton.which == SDL_JoystickInstanceID(_pad)) {
		const int button = ev.jbutton.button;
		int which = -1;

		// the core's own (J1) and the MiSTer menu's
		if (button == MISTER_JOY_CORE_OK || button == MISTER_JOY_MENU_OK)
			which = kOK;
		else if (button == MISTER_JOY_CORE_BACK || button == MISTER_JOY_MENU_BACK)
			which = kBack;
		if (which >= 0) {
			const int bit = button == MISTER_JOY_CORE_OK || button == MISTER_JOY_CORE_BACK ? 1 : 2;
			const int before = _held[which];

			if (ev.type == SDL_JOYBUTTONDOWN)
				_held[which] |= bit;
			else
				_held[which] &= ~bit;
			// two buttons may be the same one: only the first press and the
			// last release count
			if ((before != 0) == (_held[which] != 0))
				return false;
			return menuButton(which, _held[which] != 0, bit == 2 && menuActions(which) != 0, event);
		}
	}
	if ((ev.type == SDL_CONTROLLERBUTTONDOWN || ev.type == SDL_CONTROLLERBUTTONUP) && _pad &&
	    (isStickDpad(ev.cbutton) || isMenuAction(ev.cbutton)))
		return false;
	return SdlEventSource::dispatchSDLEvent(ev, event);
}

// Main_MiSTer presses the d-pad as well when a stick is pushed past half way
// (dpad_threshold in MiSTer.ini). While there is a pointer on the screen the
// left stick is the pointer and nothing else: that press goes no further, or
// the engines' actions on the d-pad came with it, Drascula's save and load
// menu with the stick pushed to the right, Queen's inventory scrolling up and
// down. Without a pointer the stick can do nothing else: Grim Fandango walks
// with it, as it does on the d-pad.
bool MisterEventSource::isStickDpad(const SDL_ControllerButtonEvent &button) {
	uint32 direction;

	switch (button.button) {
	case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
		direction = MH_JOY_RIGHT;
		break;
	case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
		direction = MH_JOY_LEFT;
		break;
	case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
		direction = MH_JOY_DOWN;
		break;
	case SDL_CONTROLLER_BUTTON_DPAD_UP:
		direction = MH_JOY_UP;
		break;
	default:
		return false;
	}

	const bool wasStick = (_stickDpad & direction) != 0;

	_stickDpad &= ~direction;
	// its release goes where its press went
	if (button.state != SDL_PRESSED)
		return wasStick;
	if (!_graphicsManager || !static_cast<MisterGraphicsManager *>(_graphicsManager)->isCursorVisible())
		return false;
	if (!(MH_StickDpad(SDL_JoystickGetAxis(_pad, 0) >> 8, SDL_JoystickGetAxis(_pad, 1) >> 8) & direction))
		return false;
	_stickDpad |= direction;
	return true;
}

// The pad's actions on the physical button of the MiSTer menu's Menu OK / Back.
// Without a mapping that is every PlayStation or Xbox pad's: the bottom button
// is Menu OK and the core's B, the right one Menu Back and the core's A. Their
// being held says nothing: the library reads Menu OK / Back from Linux, ahead
// of the core's buttons, so the pad's action comes after them.
uint32 MisterEventSource::menuActions(int which) const {
	uint32 ok, back;

	MH_MenuButtons(MH_OSDStatus(), &ok, &back);
	uint32 actions = 0;
	for (int i = 0; i < MISTER_JOY_ACTIONS; i++)
		actions |= MH_JOY_BUTTON(i);
	return (which == kOK ? ok : back) & actions;
}

// In ScummVM's dialogs a button that is Menu OK or Menu Back as well is only
// that (menuButton()), as in every port's menus: its action goes no further
bool MisterEventSource::isMenuAction(const SDL_ControllerButtonEvent &button) {
	// the pad's actions in the order of the core's J1 list (addControllerMapping())
	static const int actions[MISTER_JOY_ACTIONS] = {
		SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_X, SDL_CONTROLLER_BUTTON_Y,
		SDL_CONTROLLER_BUTTON_LEFTSHOULDER, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
		SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_BUTTON_START
	};
	uint32 action = 0;

	for (int i = 0; i < MISTER_JOY_ACTIONS; i++) {
		if (actions[i] == button.button)
			action = MH_JOY_BUTTON(i);
	}
	if (!action)
		return false;

	const bool wasMenu = (_menuAction & action) != 0;

	_menuAction &= ~action;
	// its release goes where its press went
	if (button.state != SDL_PRESSED)
		return wasMenu;
	if (!g_gui.isActive() || !((menuActions(kOK) | menuActions(kBack)) & action))
		return false;
	_menuAction |= action;
	return true;
}

// A button of the pad's eight actions is held: the Menu OK / Back pressed now
// is the same button, which already does what it does
bool MisterEventSource::actionHeld() const {
	for (int i = 0; i < MISTER_JOY_ACTIONS; i++) {
		if (SDL_JoystickGetButton(_pad, i))
			return true;
	}
	return false;
}

bool MisterEventSource::menuButton(int which, bool down, bool sameAsAction, Common::Event &event) {
	if (!down) {
		// what its press sent goes up again
		const Sent sent = _sent[which];

		_sent[which] = kSentNothing;
		switch (sent) {
		case kSentReturn:
			event.type = Common::EVENT_KEYUP;
			event.kbd = Common::KeyState(Common::KEYCODE_RETURN, Common::ASCII_RETURN);
			return true;
		case kSentEscape:
			event.type = Common::EVENT_KEYUP;
			event.kbd = Common::KeyState(Common::KEYCODE_ESCAPE, Common::ASCII_ESCAPE);
			return true;
		case kSentLeftClick:
			event.type = Common::EVENT_LBUTTONUP;
			event.mouse = g_system->getEventManager()->getMousePos();
			return true;
		default:
			return false;
		}
	}

	Sent sent = kSentNothing;
	if (g_gui.isActive()) {
		// ScummVM's dialogs: a click at the pointer on a button that is one of
		// the pad's actions as well (isMenuAction()), else Enter for the default
		// button, unless the button is the pad's left click, which confirms by
		// itself; Esc closes a dialog, but not the launcher (that would quit
		// ScummVM)
		if (which == kOK && sameAsAction)
			sent = kSentLeftClick;
		else if (which == kOK && !SDL_JoystickGetButton(_pad, MISTER_JOY_LEFT_CLICK))
			sent = kSentReturn;
		else if (which == kBack && (g_engine || g_gui.getDialogCount() > 1))
			sent = kSentEscape;
	} else if (!sameAsAction && !actionHeld()) {
		// a game's own menus are the pointer's: a click, and Esc
		sent = which == kOK ? kSentLeftClick : kSentEscape;
	}
	_sent[which] = sent;

	switch (sent) {
	case kSentReturn:
		event.type = Common::EVENT_KEYDOWN;
		event.kbd = Common::KeyState(Common::KEYCODE_RETURN, Common::ASCII_RETURN);
		return true;
	case kSentEscape:
		event.type = Common::EVENT_KEYDOWN;
		event.kbd = Common::KeyState(Common::KEYCODE_ESCAPE, Common::ASCII_ESCAPE);
		return true;
	case kSentLeftClick:
		event.type = Common::EVENT_LBUTTONDOWN;
		event.mouse = g_system->getEventManager()->getMousePos();
		return true;
	default:
		return false;
	}
}
