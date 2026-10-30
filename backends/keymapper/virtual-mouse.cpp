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

#include "backends/keymapper/virtual-mouse.h"

#include "backends/keymapper/action.h"
#include "backends/keymapper/keymap.h"

#include "common/config-manager.h"
#include "common/system.h"
#include "common/translation.h"

#include "gui/gui-manager.h"

#ifdef MISTER_HYBRID
#include <mister_hybrid.h>
#endif

namespace Common {

VirtualMouse::VirtualMouse(EventDispatcher *eventDispatcher) :
		_eventDispatcher(eventDispatcher),
		_inputAxisPositionX(0),
		_inputAxisPositionY(0),
		_mouseVelocityX(0.f),
		_mouseVelocityY(0.f),
		_slowModifier(1.f),
		_subPixelRemainderX(0.f),
		_subPixelRemainderY(0.f),
		_lastUpdateMillis(0) {
#ifdef MISTER_HYBRID
	_instance = this;
	_lastField = 0;
	_frameField = 0;
	_lastMoveField = 0;
	_moved = false;
	_warping = false;
	_dpadX = _dpadY = DpadAxis();
#endif
	ConfMan.registerDefault("kbdmouse_speed", 3);
	ConfMan.registerDefault("joystick_deadzone", 3);

	_eventDispatcher->registerSource(this, false);
	_eventDispatcher->registerObserver(this, 10, false);
}

VirtualMouse::~VirtualMouse() {
#ifdef MISTER_HYBRID
	_instance = nullptr;
#endif
	_eventDispatcher->unregisterObserver(this);
	_eventDispatcher->unregisterSource(this);
}

#ifdef MISTER_HYBRID
VirtualMouse *VirtualMouse::_instance = nullptr;

void VirtualMouse::updateBeforeFrame() {
	if (_instance) {
		_instance->movePointer(_instance->_frameField);
	}
}

void VirtualMouse::updateAfterFrame() {
	if (_instance) {
		_instance->_frameField = MH_FieldCounter();
	}
}

// Once before every frame shown, as many fields' worth as went by between the
// last two frames: the engines poll at their own rates (Lure at 50 Hz on a
// screen of 59.6 frames, and every fifth frame the pointer stood still and the
// next went twice as far), and the counter is read just after a frame, as it
// ticks: before one it may just have ticked or not. The game hears of the move
// at its next poll.
void VirtualMouse::movePointer(uint32 field) {
	if ((int32)(field - _lastField) <= 0) {
		return;
	}
	const float step = MIN<uint32>(field - _lastField, kMaxStep);
	_lastField = field;

	// Adjust the speed of the cursor according to the virtual screen resolution
	Common::Rect screenSize;
	if (g_system->isOverlayVisible()) {
		screenSize = Common::Rect(g_system->getOverlayWidth(), g_system->getOverlayHeight());
	} else {
		screenSize = Common::Rect(g_system->getWidth(), g_system->getHeight());
	}

	float screenSizeSpeedModifier = screenSize.width() / (float)kDefaultScreenWidth;

	// The stick first: Main_MiSTer presses the d-pad as well when the stick is
	// pushed past half way, and lets it go again on the way back, which must
	// not change the speed (MisterEventSource::isStickDpad() stops that press
	// while the pointer is on the screen, not with the pointer hidden)
	const uint32 curTime = g_system->getMillis(true);
	const bool stickMoves = _mouseVelocityX != 0.f || _mouseVelocityY != 0.f;
	const float velocityX = stickMoves ? _mouseVelocityX : dpadVelocity(_dpadX, curTime);
	const float velocityY = stickMoves ? _mouseVelocityY : dpadVelocity(_dpadY, curTime);

	// Nothing left over from before, or a tap of the d-pad could be a pixel more
	if (velocityX == 0.f) {
		_subPixelRemainderX = 0.f;
	}
	if (velocityY == 0.f) {
		_subPixelRemainderY = 0.f;
	}

	float deltaX = _subPixelRemainderX + velocityX * _slowModifier * screenSizeSpeedModifier * 10.f * step;
	float deltaY = _subPixelRemainderY + velocityY * _slowModifier * screenSizeSpeedModifier * 10.f * step;

	Common::Point delta;
	delta.x = deltaX;
	delta.y = deltaY;

	// Keep track of sub-pixel movement so the cursor ultimately moves,
	// even when configured at very low speeds.
	_subPixelRemainderX = deltaX - delta.x;
	_subPixelRemainderY = deltaY - delta.y;

	delta.x += _dpadX.firstPixels;
	delta.y += _dpadY.firstPixels;
	_dpadX.firstPixels = _dpadY.firstPixels = 0;

	if (delta.x == 0 && delta.y == 0) {
		return;
	}

	// During a stroke from where the last move went, which the game may not have
	// heard of yet (warpMouse() throws away what is queued)
	Common::Point pos = (field - _lastMoveField < kMaxStep ? _movedTo : g_system->getEventManager()->getMousePos()) + delta;

	// The last pixel: a game's x of 320 at 320x200 is off its screen
	pos.x = CLIP<int16>(pos.x, 0, screenSize.width() - 1);
	pos.y = CLIP<int16>(pos.y, 0, screenSize.height() - 1);

	_movedBy += delta;
	_movedTo = pos;
	_moved = true;
	_lastMoveField = field;

	// It polls the event sources: not this one, or the move is thrown away
	_warping = true;
	g_system->warpMouse(pos.x, pos.y);
	_warping = false;
}

bool VirtualMouse::pollEvent(Event &event) {
	if (_warping) {
		return false;
	}

	// An engine that has stopped showing frames for a while
	const uint32 field = MH_FieldCounter();
	if (field - _frameField > kMaxStep) {
		movePointer(field);
	}
	if (!_moved) {
		return false;
	}

	// Send a mouse event
	event.type = Common::EVENT_MOUSEMOVE;
	event.mouse = _movedTo;
	event.relMouse = _movedBy;

	_moved = false;
	_movedBy = Common::Point();

	return true;
}
#else
bool VirtualMouse::pollEvent(Event &event) {
	// Update the virtual mouse once per frame (assuming 60Hz)
	uint32 curTime = g_system->getMillis(true);
	if (curTime < _lastUpdateMillis + kUpdateDelay) {
		return false;
	}
	_lastUpdateMillis = curTime;

	// Adjust the speed of the cursor according to the virtual screen resolution
	Common::Rect screenSize;
	if (g_system->isOverlayVisible()) {
		screenSize = Common::Rect(g_system->getOverlayWidth(), g_system->getOverlayHeight());
	} else {
		screenSize = Common::Rect(g_system->getWidth(), g_system->getHeight());
	}

	float screenSizeSpeedModifier = screenSize.width() / (float)kDefaultScreenWidth;

	// Compute the movement delta when compared to the previous update
	float deltaX = _subPixelRemainderX + _mouseVelocityX * _slowModifier * screenSizeSpeedModifier * 10.f;
	float deltaY = _subPixelRemainderY + _mouseVelocityY * _slowModifier * screenSizeSpeedModifier * 10.f;

	Common::Point delta;
	delta.x = deltaX;
	delta.y = deltaY;

	// Keep track of sub-pixel movement so the cursor ultimately moves,
	// even when configured at very low speeds.
	_subPixelRemainderX = deltaX - delta.x;
	_subPixelRemainderY = deltaY - delta.y;

	if (delta.x == 0 && delta.y == 0) {
		return false;
	}

	// Send a mouse event
	Common::Point oldPos = g_system->getEventManager()->getMousePos();

	event.type = Common::EVENT_MOUSEMOVE;
	event.mouse = oldPos + delta;

	event.mouse.x = CLIP<int16>(event.mouse.x, 0, screenSize.width());
	event.mouse.y = CLIP<int16>(event.mouse.y, 0, screenSize.height());

	event.relMouse.x = delta.x;
	event.relMouse.y = delta.y;

	g_system->warpMouse(event.mouse.x, event.mouse.y);

	return true;
}
#endif

bool VirtualMouse::notifyEvent(const Event &event) {
	if (event.type != EVENT_CUSTOM_BACKEND_ACTION_AXIS) {
		return false;
	}

	switch (event.customType) {
	case kCustomActionVirtualAxisUp:
		if (event.joystick.position == 0 && _inputAxisPositionY > 0) {
			return true; // Ignore axis reset events if we are already going in the other direction
		}

		handleAxisMotion(_inputAxisPositionX, -event.joystick.position);
		return true;
	case kCustomActionVirtualAxisDown:
		if (event.joystick.position == 0 && _inputAxisPositionY < 0) {
			return true;
		}

		handleAxisMotion(_inputAxisPositionX, event.joystick.position);
		return true;
	case kCustomActionVirtualAxisLeft:
		if (event.joystick.position == 0 && _inputAxisPositionX > 0) {
			return true;
		}

		handleAxisMotion(-event.joystick.position, _inputAxisPositionY);
		return true;
	case kCustomActionVirtualAxisRight:
		if (event.joystick.position == 0 && _inputAxisPositionX < 0) {
			return true;
		}

		handleAxisMotion(event.joystick.position, _inputAxisPositionY);
		return true;
	case kCustomActionVirtualMouseSlow:
		_slowModifier = 0.9f * (1.f - event.joystick.position / (float)JOYAXIS_MAX) + 0.1f;
		return true;
#ifdef MISTER_HYBRID
	case kCustomActionVirtualDpadUp:
		handleDpad(_dpadY, _dpadY.negative, -1, event.joystick.position != 0);
		return true;
	case kCustomActionVirtualDpadDown:
		handleDpad(_dpadY, _dpadY.positive, 1, event.joystick.position != 0);
		return true;
	case kCustomActionVirtualDpadLeft:
		handleDpad(_dpadX, _dpadX.negative, -1, event.joystick.position != 0);
		return true;
	case kCustomActionVirtualDpadRight:
		handleDpad(_dpadX, _dpadX.positive, 1, event.joystick.position != 0);
		return true;
#endif
	default:
		break;
	}

	return false;
}

void VirtualMouse::addActionsToKeymap(Keymap *keymap) {
	Action *act;

	act = new Action("VMOUSEUP", _("Virtual mouse up"));
	act->addDefaultInputMapping("JOY_LEFT_STICK_Y-");
	act->setCustomBackendActionAxisEvent(VirtualMouse::kCustomActionVirtualAxisUp);
	keymap->addAction(act);

	act = new Action("VMOUSEDOWN", _("Virtual mouse down"));
	act->addDefaultInputMapping("JOY_LEFT_STICK_Y+");
	act->setCustomBackendActionAxisEvent(VirtualMouse::kCustomActionVirtualAxisDown);
	keymap->addAction(act);

	act = new Action("VMOUSELEFT", _("Virtual mouse left"));
	act->addDefaultInputMapping("JOY_LEFT_STICK_X-");
	act->setCustomBackendActionAxisEvent(VirtualMouse::kCustomActionVirtualAxisLeft);
	keymap->addAction(act);

	act = new Action("VMOUSERIGHT", _("Virtual mouse right"));
	act->addDefaultInputMapping("JOY_LEFT_STICK_X+");
	act->setCustomBackendActionAxisEvent(VirtualMouse::kCustomActionVirtualAxisRight);
	keymap->addAction(act);

	act = new Action("VMOUSESLOW", _("Slow down virtual mouse"));
	act->addDefaultInputMapping("JOY_RIGHT_SHOULDER");
	act->setCustomBackendActionAxisEvent(VirtualMouse::kCustomActionVirtualMouseSlow);
	keymap->addAction(act);

#ifdef MISTER_HYBRID
	act = new Action("VMOUSEDPADUP", _("Virtual mouse up (d-pad)"));
	act->setCustomBackendActionAxisEvent(VirtualMouse::kCustomActionVirtualDpadUp);
	keymap->addAction(act);

	act = new Action("VMOUSEDPADDOWN", _("Virtual mouse down (d-pad)"));
	act->setCustomBackendActionAxisEvent(VirtualMouse::kCustomActionVirtualDpadDown);
	keymap->addAction(act);

	act = new Action("VMOUSEDPADLEFT", _("Virtual mouse left (d-pad)"));
	act->setCustomBackendActionAxisEvent(VirtualMouse::kCustomActionVirtualDpadLeft);
	keymap->addAction(act);

	act = new Action("VMOUSEDPADRIGHT", _("Virtual mouse right (d-pad)"));
	act->setCustomBackendActionAxisEvent(VirtualMouse::kCustomActionVirtualDpadRight);
	keymap->addAction(act);
#endif
}

void VirtualMouse::handleAxisMotion(int16 axisPositionX, int16 axisPositionY) {
	_inputAxisPositionX = axisPositionX;
	_inputAxisPositionY = axisPositionY;

	float analogX  = (float)_inputAxisPositionX;
	float analogY  = (float)_inputAxisPositionY;
	float deadZone = (float)ConfMan.getInt("joystick_deadzone") * 1000.0f;

	float magnitude = sqrt(analogX * analogX + analogY * analogY);

	if (magnitude >= deadZone) {
#ifdef MISTER_HYBRID
		// Squared, for the small movements a point and click game needs: a
		// quarter of the speed at half way out, full speed at the edge, and a
		// stick that goes further on the diagonal no faster than straight on
		float tilt = MIN((magnitude - deadZone) / (JOYAXIS_MAX - deadZone), 1.f);
		float scalingFactor = 1.0f / magnitude * tilt * tilt;
#else
		float scalingFactor = 1.0f / magnitude * (magnitude - deadZone) / (JOYAXIS_MAX - deadZone);
#endif
		float speedFactor = computeJoystickMouseSpeedFactor();
		_mouseVelocityX = analogX * scalingFactor * speedFactor;
		_mouseVelocityY = analogY * scalingFactor * speedFactor;
	} else {
		_mouseVelocityX = 0.f;
		_mouseVelocityY = 0.f;
	}
}

#ifdef MISTER_HYBRID
void VirtualMouse::handleDpad(DpadAxis &axis, bool &direction, int sign, bool down) {
	direction = down;
	axis.start = g_system->getMillis(true);
	// The tap's pixels at once, however short the tap (with Slow Pointer held
	// one, to reach every pixel), but not with the stick moving the pointer
	// (pollEvent())
	if (down && _mouseVelocityX == 0.f && _mouseVelocityY == 0.f) {
		axis.firstPixels = sign * (_slowModifier < 1.f ? 1 : kDpadTap);
	}
}

float VirtualMouse::dpadVelocity(DpadAxis &axis, uint32 curTime) const {
	const int direction = (axis.positive ? 1 : 0) - (axis.negative ? 1 : 0);
	const uint32 held = curTime - axis.start;

	if (direction == 0 || held < kDpadRampDelay) {
		return 0.f;
	}
	const float ramp = MIN((held - kDpadRampDelay) / (float)kDpadRampTime, 1.f);
	return direction * (0.1f + 0.9f * ramp * ramp) * computeJoystickMouseSpeedFactor();
}

#endif

float VirtualMouse::computeJoystickMouseSpeedFactor() const {
	switch (ConfMan.getInt("kbdmouse_speed")) {
	case 0:
		return 0.25; // 0.25 keyboard pointer speed
	case 1:
		return 0.5;  // 0.5 speed
	case 2:
		return 0.75; // 0.75 speed
	case 3:
		return 1.0;  // 1.0 speed
	case 4:
		return 1.25; // 1.25 speed
	case 5:
		return 1.5;  // 1.5 speed
	case 6:
		return 1.75; // 1.75 speed
	case 7:
		return 2.0;  // 2.0 speed
	default:
		return 1.0;
	}
}

} // End of namespace Common
