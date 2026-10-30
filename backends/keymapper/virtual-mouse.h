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

#ifndef BACKENDS_KEYMAPPER_VIRTUAL_MOUSE_H
#define BACKENDS_KEYMAPPER_VIRTUAL_MOUSE_H

#include "common/scummsys.h"

#include "common/events.h"

namespace Common {

class EventDispatcher;
class Keymap;

/**
 * The Virtual Mouse can produce mouse move events on systems without a physical mouse.
 *
 * It is useful for moving the mouse cursor using a gamepad or a keyboard.
 *
 * This class defines a keymap with actions for moving the cursor in all four directions.
 * The keymapper produces custom backend events whenever keys bound to these actions are
 * pressed. This class handles the events through its EventObserver interface and produces
 * mouse move events when necesssary through its EventSource interface.
 */
class VirtualMouse : public EventSource, public EventObserver {
public:
	VirtualMouse(EventDispatcher *eventDispatcher);
	~VirtualMouse() override;

	// EventSource API
	bool pollEvent(Event &event) override;

	// EventObserver API
	bool notifyEvent(const Event &event) override;

	/** Add the virtual mouse keymapper actions to a keymap */
	void addActionsToKeymap(Keymap *keymap);

private:
	static const int32 kUpdateDelay = 12;
	static const int32 kDefaultScreenWidth = 640;

	enum {
		kCustomActionVirtualAxisUp    = 10000,
		kCustomActionVirtualAxisDown  = 10001,
		kCustomActionVirtualAxisLeft  = 10002,
		kCustomActionVirtualAxisRight = 10003,
		kCustomActionVirtualMouseSlow = 10004
#ifdef MISTER_HYBRID
		,
		kCustomActionVirtualDpadUp    = 10005,
		kCustomActionVirtualDpadDown  = 10006,
		kCustomActionVirtualDpadLeft  = 10007,
		kCustomActionVirtualDpadRight = 10008
#endif
	};

	void handleAxisMotion(int16 axisPositionX, int16 axisPositionY);
	float computeJoystickMouseSpeedFactor() const;

#ifdef MISTER_HYBRID
	// The d-pad, apart from the stick: a tap moves the pointer one pixel, held
	// it starts slow and gets faster, up to the stick's full speed
	static const uint32 kDpadRampDelay = 150; // ms before it moves on by itself
	static const uint32 kDpadRampTime = 850;  // ms from then to its full speed
	static const uint32 kMaxStep = 100;       // ms a slow frame or a load counts for

	struct DpadAxis {
		bool negative;
		bool positive;
		uint32 start;
		int firstPixel;
	};

	void handleDpad(DpadAxis &axis, bool &direction, int sign, bool down);
	float dpadVelocity(DpadAxis &axis, uint32 curTime) const;

	DpadAxis _dpadX;
	DpadAxis _dpadY;
#endif

	EventDispatcher *_eventDispatcher;

	int16 _inputAxisPositionX;
	int16 _inputAxisPositionY;

	float _mouseVelocityX;
	float _mouseVelocityY;
	float _slowModifier;

	float _subPixelRemainderX;
	float _subPixelRemainderY;

	uint32 _lastUpdateMillis;
};

} // End of namespace Common

#endif // #ifndef BACKENDS_KEYMAPPER_VIRTUAL_MOUSE_H
