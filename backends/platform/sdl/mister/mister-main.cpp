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
#include "backends/platform/sdl/mister/mister_port.h"
#include "backends/plugins/sdl/sdl-provider.h"
#include "base/main.h"

#include <stdlib.h>

int main(int argc, char *argv[]) {
	// The core's intro, then ScummVM's logo while it loads
	if (!MiSTer_Start())
		return MiSTer_ExitCode();

	// SDL's "mister" drivers (hybrid/sdl2): at most one frame per field, as
	// the core shows them (one per two fields in the interlaced modes)
	setenv("SDL_MISTER_VSYNC", "1", 0);
	// ScummVM draws into the library's frame buffer (mister-graphics.cpp)
	setenv("SDL_MISTER_DIRECT", "1", 0);

	MiSTer_ProfStart();

	g_system = new OSystem_SDL_MiSTer();
	assert(g_system);

	// Pre initialize the backend
	g_system->init();

#ifdef DYNAMIC_MODULES
	PluginManager::instance().addPluginProvider(new SDLPluginProvider());
#endif

	// Invoke the actual ScummVM main entry point
	int res = scummvm_main(argc, argv);

	// Free OSystem
	g_system->destroy();

	return res ? res : MiSTer_ExitCode();
}
