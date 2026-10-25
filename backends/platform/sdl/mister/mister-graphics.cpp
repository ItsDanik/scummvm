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

#include "backends/platform/sdl/mister/mister-graphics.h"
#include "backends/platform/sdl/mister/mister_port.h"
#include "backends/platform/sdl/sdl-window.h"
#include "engines/engine.h"

MisterGraphicsManager::MisterGraphicsManager(SdlEventSource *sdlEventSource, SdlWindow *window)
	: SurfaceSdlGraphicsManager(sdlEventSource, window) {
}

bool MisterGraphicsManager::hasFeature(OSystem::Feature f) const {
	switch (f) {
	// the core's screen is all there is: no window, and nothing to scale,
	// stretch, filter or turn (the OSD's video options are the core's own)
	case OSystem::kFeatureFullscreenMode:
	case OSystem::kFeatureScalers:
	case OSystem::kFeatureAspectRatioCorrection:
	case OSystem::kFeatureFilteringMode:
	case OSystem::kFeatureStretchMode:
	case OSystem::kFeatureRotationMode:
	case OSystem::kFeatureVSync:
	case OSystem::kFeatureIconifyWindow:
		return false;
	default:
		return SurfaceSdlGraphicsManager::hasFeature(f);
	}
}

void MisterGraphicsManager::setFeatureState(OSystem::Feature f, bool enable) {
	if (hasFeature(f))
		SurfaceSdlGraphicsManager::setFeatureState(f, enable);
}

bool MisterGraphicsManager::getFeatureState(OSystem::Feature f) const {
	return hasFeature(f) && SurfaceSdlGraphicsManager::getFeatureState(f);
}

// With no game running the size is the launcher's, which ScummVM asks for
// as 320x200 (base/main.cpp): the OSD's Launcher line can make it 640x480,
// ScummVM's high resolution layout. Games get the size they ask for.
void MisterGraphicsManager::initSize(uint w, uint h, const Graphics::PixelFormat *format) {
	if (!g_engine && MiSTer_LauncherHires()) {
		w = 640;
		h = 480;
	}
	SurfaceSdlGraphicsManager::initSize(w, h, format);
}

uint MisterGraphicsManager::getDefaultScaleFactor() const {
	return 1;
}

bool MisterGraphicsManager::setScaler(uint mode, int factor) {
	// whatever a game's settings say: 1x
	return SurfaceSdlGraphicsManager::setScaler(getDefaultScaler(), 1);
}

// A CRT hides its picture's edges (Danik, 2026-10-25): ScummVM's GUI keeps 10
// pixels from them at 640x400 / 640x480 and 5 at 320x200 (the same width on
// the screen), every dialog, popup and message; the backgrounds of those
// that fill the screen still reach its edges (gui/ThemeLayout.cpp). The
// layouts are still picked by the whole size (MISTER_HYBRID in
// gui/gui-manager.cpp): ScummVM sizes its GUI by this area otherwise, and
// would make 620x380 of 640x400 the low resolution layout and scale the one
// of 320x200 by 0.95.
Common::Rect MisterGraphicsManager::getSafeOverlayArea(int16 *width, int16 *height) const {
	const int16 w = getOverlayWidth(), h = getOverlayHeight();
	const int16 m = w >= 640 ? 10 : 5;
	if (width)
		*width = w;
	if (height)
		*height = h;
	return Common::Rect(m, m, w - m, h - m);
}

// The game's own size: a 15kHz screen shows 200 lines as 4:3 by itself, so
// no aspect ratio correction (ScummVM's default, which made 320x240 of them)
void MisterGraphicsManager::setupHardwareSize() {
	_videoMode.aspectRatioCorrection = false;
	_videoMode.scaleFactor = 1;
	SurfaceSdlGraphicsManager::setupHardwareSize();
}

// The window surface is the library's own frame buffer (SDL_MISTER_DIRECT,
// mister-main.cpp), which holds the frame before last: what changed in the
// last two frames is drawn, as ScummVM does for a double buffered screen
// (backends/graphics/surfacesdl, MISTER_HYBRID: the two buffers take turns).
// One copy of each frame less, a frame's time on the ARM's memory for a
// 640x480 game. (A window of another size than the core's mode has the
// driver's own surface, copied.)
void MisterGraphicsManager::initGraphicsSurface() {
	SurfaceSdlGraphicsManager::initGraphicsSurface();
	_isDoubleBuf = true;
}

// The window is the frame, exactly: its size picks the core's mode
// (hybrid/sdl2/SDL_mistervideo.c), and its own surface is the frame the
// core gets, so no renderer and no texture in between
SDL_Surface *MisterGraphicsManager::SDL_SetVideoMode(int width, int height, int bpp, Uint32 flags) {
	SDL_Window *sdlWindow;

	deinitializeRenderer();
	if (!_window->getSDLWindow() && !_window->createOrUpdateWindow(width, height, 0))
		return nullptr;
	sdlWindow = _window->getSDLWindow();
	// not limited to the "desktop", as createOrUpdateWindow() does
	SDL_SetWindowSize(sdlWindow, width, height);

	getWindowSizeFromSdl(&_windowWidth, &_windowHeight);
	handleResize(_windowWidth, _windowHeight);

	return SDL_GetWindowSurface(sdlWindow);
}

void MisterGraphicsManager::SDL_UpdateRects(SDL_Surface *screen, int numrects, SDL_Rect *rects) {
	SDL_UpdateWindowSurface(_window->getSDLWindow());
	MiSTer_ProfFrame();
}
