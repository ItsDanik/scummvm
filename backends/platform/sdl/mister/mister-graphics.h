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

#ifndef BACKENDS_GRAPHICS_MISTER_H
#define BACKENDS_GRAPHICS_MISTER_H

#include "backends/graphics/surfacesdl/surfacesdl-graphics.h"

// The game's screen as the MiSTer core shows it: the window is the game's
// size, unscaled (the window's size picks the core's video mode: 320x200,
// 640x480 interlaced at 15kHz, ...; other sizes are centred in the smallest
// mode they fit in), and its surface is the frame the core gets, RGB565. No
// renderer, scaler, aspect ratio correction or full screen: a 15kHz screen
// is 4:3 by itself. The GUI is drawn at the game's size (ScummVM's low
// resolution layout in a 320x200 game), so opening it changes no mode.
class MisterGraphicsManager : public SurfaceSdlGraphicsManager {
public:
	MisterGraphicsManager(SdlEventSource *sdlEventSource, SdlWindow *window);

	bool hasFeature(OSystem::Feature f) const override;
	void setFeatureState(OSystem::Feature f, bool enable) override;
	bool getFeatureState(OSystem::Feature f) const override;

	void initSize(uint w, uint h, const Graphics::PixelFormat *format = NULL) override;

	uint getDefaultScaleFactor() const override;
	bool setScaler(uint mode, int factor) override;

	Common::Rect getSafeOverlayArea(int16 *width, int16 *height) const override;

protected:
	void setupHardwareSize() override;
	void initGraphicsSurface() override;
	SDL_Surface *SDL_SetVideoMode(int width, int height, int bpp, Uint32 flags) override;
	void SDL_UpdateRects(SDL_Surface *screen, int numrects, SDL_Rect *rects) override;
};

#endif
