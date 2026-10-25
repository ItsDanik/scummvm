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

#include "audio/fmopl.h"
#include "backends/platform/sdl/mister/mister-mixer.h"

#include <mister_hybrid.h>
#include <string.h>

namespace OPL {
namespace MiSTer {

// The YM3812 (OPL2) in the MiSTer core (hybrid/README.md, "The FM chip"):
// the music's register writes go to the core, which plays the chip's sound
// with the rest of the audio. Nothing of the chip is worked out on the ARM.
// It is an emulated chip all the same, a silent stream in the mixer: the
// music's timer runs in it, so each write made by the timer is made at its
// audio frame (MH_FMWrite()'s "ahead"), as with an emulator.
class OPL : public ::OPL::OPL, public Audio::EmulatedChip {
public:
	~OPL() override {
		stop();
		MH_FMEnable(0);
	}

	bool init() override {
		MH_FMEnable(1);
		MH_FMVolume(32);
		_reg = 0;
		return true;
	}

	void reset() override {
		// enabling it again resets the chip and clears its registers
		MH_FMEnable(1);
		_reg = 0;
	}

	void write(int a, int v) override {
		// even ports take the register's number, odd ones its value
		if ((a & 1) == 0)
			_reg = v & 0xFF;
		else
			writeReg(_reg, v);
	}

	void writeReg(int r, int v) override {
		MH_FMWrite(r & 0xFF, v & 0xFF, _clock.ahead());
	}

	bool isStereo() const override { return false; }

protected:
	void generateSamples(int16 *buffer, int numSamples) override {
		memset(buffer, 0, numSamples * sizeof(int16));
		_clock.advance(numSamples);
	}

private:
	int _reg = 0;
	MisterAudioClock _clock;
};

::OPL::OPL *create(Config::OplType type) {
	// a core without the chip (older than host version 10, or the PC without
	// fakecore): an emulator instead
	if (type != Config::kOpl2 || !MH_FMAvailable())
		return nullptr;
	return new OPL();
}

} // End of namespace MiSTer
} // End of namespace OPL
