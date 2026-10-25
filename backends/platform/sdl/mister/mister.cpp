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

#include "backends/platform/sdl/mister/mister.h"
#include "backends/platform/sdl/mister/mister-events.h"
#include "backends/platform/sdl/mister/mister-graphics.h"
#include "backends/platform/sdl/mister/mister-mixer.h"
#include "backends/platform/sdl/mister/mister_port.h"

#include "backends/keymapper/keymapper.h"
#include "backends/keymapper/keymapper-defaults.h"
#include "backends/mixer/null/null-mixer.h"
#include "common/config-manager.h"
#include "common/debug.h"
#include "engines/engine.h"

#include <mister_hybrid.h>
#include <limits.h>
#include <unistd.h>

// The folder of the program (in data/ the themes, engine data and the
// virtual keyboard: what comes with the port, games/dnk/ScummVM on the SD card)
static Common::Path programDir() {
	char path[PATH_MAX];
	const ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);

	if (len <= 0)
		return Common::Path(".");
	path[len] = '\0';
	return Common::Path(path, '/').getParent();
}

// The folder ScummVM runs in (its settings, saved games and the player's
// games: games/dnk/ScummVM, on a USB drive if the player put it there)
static Common::Path dataDir() {
	char path[PATH_MAX];

	if (!getcwd(path, sizeof(path)))
		return Common::Path(".");
	return Common::Path(path, '/');
}

void OSystem_SDL_MiSTer::initBackend() {
	const Common::Path program = programDir();
	const Common::Path data = dataDir();

	// what comes with the port: in the session's settings, as the command
	// line's (they are looked for with hasKey(), which defaults are not)
	const char *const session = Common::ConfigManager::kSessionDomain;
	ConfMan.setPath("themepath", program.join("data"), session);
	ConfMan.setPath("extrapath", program.join("data"), session);
	ConfMan.setPath("vkeybdpath", program.join("data"), session);
	ConfMan.registerDefault("savepath", data.join("saves"));
	ConfMan.registerDefault("browser_lastpath", data);
	// the port's theme, in the purple screen's colours and font
	// (tools/make_theme.py of ScummVM_MiSTer); bitmap fonts at their own size:
	// the GUI is 1x, and ScummVM's scaler of them crashes on its Cozette.
	// The theme goes into the settings: the GUI registers a default of its
	// own ("scummremastered", gui/gui-manager.cpp) over the backend's
	if (!ConfMan.hasKey("gui_theme", Common::ConfigManager::kApplicationDomain))
		ConfMan.set("gui_theme", "misterhybrid", Common::ConfigManager::kApplicationDomain);
	ConfMan.registerDefault("gui_disable_fixed_font_scaling", true);
	ConfMan.registerDefault("output_rate", 44100);
	// the core's FM chip and MIDI synthesizer
	ConfMan.registerDefault("opl_driver", "mister");
	ConfMan.registerDefault("gm_device", "mister");
	ConfMan.registerDefault("mt32_device", "mister");
	ConfMan.registerDefault("joystick_deadzone", 3);
	// TinyGL (Grim Fandango) draws the whole frame each time: its dirty
	// rectangles cost more than they save once anything moves (Manny
	// walking: 92 to 143ms a frame against 75 on the ARM)
	ConfMan.registerDefault("dirtyrects", false);
	// the pad is the core's joystick 1, whatever the settings say
	ConfMan.setInt("joystick_num", 0, Common::ConfigManager::kTransientDomain);

	if (!_eventSource) {
		MisterEventSource::addControllerMapping();
		_eventSource = new MisterEventSource(this);
	}
	if (!_graphicsManager)
		_graphicsManager = new MisterGraphicsManager(_eventSource, _window);
	if (!_mixerManager) {
		_mixerManager = new MisterMixerManager();
		_mixerManager->init();
		if (!_mixerManager->getMixer()) {
			delete _mixerManager;
			_mixerManager = new NullMixerManager();
			_mixerManager->init();
		}
	}

	OSystem_POSIX::initBackend();

	MiSTer_SetScummRunning(false);
	applyOSD(true);
}

bool OSystem_SDL_MiSTer::hasFeature(Feature f) {
	switch (f) {
	// nothing to open them with
	case kFeatureDisplayLogFile:
	case kFeatureOpenUrl:
	case kFeatureSystemBrowserDialog:
		return false;
	case kFeatureKbdMouseSpeed:
		return true;
	default:
		return OSystem_POSIX::hasFeature(f);
	}
}

void OSystem_SDL_MiSTer::engineInit() {
	OSystem_POSIX::engineInit();
	applyMusicDevice();
	MiSTer_SetScummRunning(ConfMan.get("engineid") == "scumm");
}

void OSystem_SDL_MiSTer::engineDone() {
	OSystem_POSIX::engineDone();
	MiSTer_SetScummRunning(false);
}

// Where the music goes is the OSD's (Music: MIDI / FM chip), over the game's
// own audio options. An engine picks its driver once, when the game starts:
// a change of the OSD's Music line takes effect at the next start.
void OSystem_SDL_MiSTer::applyMusicDevice() {
	const char *const domain = Common::ConfigManager::kTransientDomain;

	if (MiSTer_MusicMidi() && MH_MidiWanted()) {
		ConfMan.set("music_driver", "mister", domain);
		// an MT32-pi in MT-32 mode gets the games' own MT-32 music
		ConfMan.setBool("native_mt32", MiSTer_PiMT32(), domain);
	} else {
		ConfMan.set("music_driver", "adlib", domain);
		ConfMan.setBool("native_mt32", false, domain);
	}
	debug(1, "MiSTer: music to %s%s", ConfMan.get("music_driver", domain).c_str(),
	      ConfMan.getBool("native_mt32", domain) ? " (MT-32)" : "");
}

void OSystem_SDL_MiSTer::applyOSD(bool force) {
	static const int pointerSpeeds[] = { 3, 1, 5 }; // kbdmouse_speed: Normal, Slow, Fast
	const char *const domain = Common::ConfigManager::kTransientDomain;
	const int pointer = MiSTer_PointerSpeed();
	const int sound = MiSTer_SoundVolume();
	const int music = MiSTer_MusicVolume();
	const int speech = MiSTer_SpeechVolume();
	const int subtitles = MiSTer_Subtitles();
	const int launcher = MiSTer_LauncherHires();
	bool sync = false;

	// the launcher's size (MisterGraphicsManager::initSize()) changes at
	// once while it is on the screen; ScummVM's GUI sees the new size and
	// takes the other layout. While a game runs, at the return to it.
	if (launcher != _lastLauncher) {
		if (!force && !g_engine) {
			beginGFXTransaction();
			initSize(320, 200);
			endGFXTransaction();
		}
		_lastLauncher = launcher;
	}

	if (force || pointer != _lastPointer) {
		ConfMan.setInt("kbdmouse_speed", pointerSpeeds[pointer > 2 ? 0 : pointer], domain);
		_lastPointer = pointer;
	}
	if (force || sound != _lastSound || music != _lastMusic || speech != _lastSpeech) {
		ConfMan.setInt("sfx_volume", sound, domain);
		ConfMan.setInt("music_volume", music, domain);
		ConfMan.setInt("speech_volume", speech, domain);
		_lastSound = sound;
		_lastMusic = music;
		_lastSpeech = speech;
		sync = true;
	}
	if (force || subtitles != _lastSubtitles) {
		switch (subtitles) {
		case MISTER_SUBTITLES_SPEECH:
			ConfMan.setBool("subtitles", false, domain);
			ConfMan.setBool("speech_mute", false, domain);
			break;
		case MISTER_SUBTITLES_TEXT:
			ConfMan.setBool("subtitles", true, domain);
			ConfMan.setBool("speech_mute", true, domain);
			break;
		case MISTER_SUBTITLES_BOTH:
			ConfMan.setBool("subtitles", true, domain);
			ConfMan.setBool("speech_mute", false, domain);
			break;
		default: // the game's own setting
			ConfMan.removeKey("subtitles", domain);
			ConfMan.removeKey("speech_mute", domain);
			break;
		}
		_lastSubtitles = subtitles;
		sync = true;
	}
	if (!sync)
		return;
	if (g_engine && !force) {
		g_engine->syncSoundSettings();
	} else if (Audio::Mixer *mixer = getMixer()) {
		mixer->setVolumeForSoundType(Audio::Mixer::kSFXSoundType, sound);
		mixer->setVolumeForSoundType(Audio::Mixer::kMusicSoundType, music);
		mixer->setVolumeForSoundType(Audio::Mixer::kSpeechSoundType, speech);
	}
}

// The pad (hybrid/README.md, "Controls"; the core's J1 list names the same
// actions): the left stick and the d-pad move the pointer, L held slows it
// down; A left click, B right click, X Esc (skip), Y "." (skip a line), R
// the game's menu (F5), Select the virtual keyboard, Start ScummVM's menu.
// Engines with pad controls of their own (Grim Fandango) keep theirs.
Common::KeymapperDefaultBindings *OSystem_SDL_MiSTer::getKeymapperDefaultBindings() {
	Common::KeymapperDefaultBindings *bindings = new Common::KeymapperDefaultBindings();

	bindings->setDefaultBinding(Common::kGlobalKeymapName, "VMOUSEUP", "JOY_LEFT_STICK_Y-");
	bindings->addDefaultBinding(Common::kGlobalKeymapName, "VMOUSEUP", "JOY_UP");
	bindings->setDefaultBinding(Common::kGlobalKeymapName, "VMOUSEDOWN", "JOY_LEFT_STICK_Y+");
	bindings->addDefaultBinding(Common::kGlobalKeymapName, "VMOUSEDOWN", "JOY_DOWN");
	bindings->setDefaultBinding(Common::kGlobalKeymapName, "VMOUSELEFT", "JOY_LEFT_STICK_X-");
	bindings->addDefaultBinding(Common::kGlobalKeymapName, "VMOUSELEFT", "JOY_LEFT");
	bindings->setDefaultBinding(Common::kGlobalKeymapName, "VMOUSERIGHT", "JOY_LEFT_STICK_X+");
	bindings->addDefaultBinding(Common::kGlobalKeymapName, "VMOUSERIGHT", "JOY_RIGHT");
	bindings->setDefaultBinding(Common::kGlobalKeymapName, "VMOUSESLOW", "JOY_LEFT_SHOULDER");

	// the d-pad is the pointer in the GUI and the games too
	bindings->setDefaultBinding(Common::kGuiKeymapName, "UP", "");
	bindings->setDefaultBinding(Common::kGuiKeymapName, "DOWN", "");
	bindings->setDefaultBinding(Common::kGuiKeymapName, "LEFT", "");
	bindings->setDefaultBinding(Common::kGuiKeymapName, "RIGHT", "");
	bindings->setDefaultBinding("engine-default", "UP", "");
	bindings->setDefaultBinding("engine-default", "DOWN", "");
	bindings->setDefaultBinding("engine-default", "LEFT", "");
	bindings->setDefaultBinding("engine-default", "RIGHT", "");

	bindings->setDefaultBinding("engine-default", "SKIP", "JOY_X");
	bindings->addDefaultBinding("engine-default", "SKIP", "ESCAPE");
	bindings->setDefaultBinding("engine-default", "SKLI", "JOY_Y");
	bindings->addDefaultBinding("engine-default", "SKLI", "PERIOD");
	bindings->setDefaultBinding("engine-default", "MENU", "JOY_RIGHT_SHOULDER");
	bindings->addDefaultBinding("engine-default", "MENU", "F5");

	return bindings;
}

// Settings, saved games and the log stay in the folder ScummVM runs in; the
// log only while it runs (/tmp: the SD card is mounted "sync", and every line
// written to it would stop the game)
Common::Path OSystem_SDL_MiSTer::getDefaultConfigFileName() {
	return dataDir().join("scummvm.ini");
}

Common::Path OSystem_SDL_MiSTer::getDefaultLogFileName() {
	return Common::Path("/tmp/scummvm_mister.log");
}
