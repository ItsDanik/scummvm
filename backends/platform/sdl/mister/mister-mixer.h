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

#ifndef BACKENDS_MIXER_MISTER_H
#define BACKENDS_MIXER_MISTER_H

#include "backends/mixer/sdl/sdl-mixer.h"

// The mixer, through SDL's "mister" audio driver: 44.1kHz stereo, as the core
// plays it. It counts the blocks it mixes, so that the FM chip's register
// writes and the MIDI messages made while one is mixed (by the music's timer,
// which runs in the audio: MisterAudioClock) are made at their audio frame of
// that block (MH_FMWrite(), MH_MidiEvent()). The MIDI synthesizer is enabled
// with the audio.
class MisterMixerManager : public SdlMixerManager {
public:
	void init() override;

protected:
	void callbackHandler(byte *samples, int len) override;
};

// Where in the block being mixed a stream that makes FM or MIDI music is: one
// for each such stream, which counts the frames it has made of the block
class MisterAudioClock {
public:
	// The stream makes `frames` more frames of the block
	void advance(int frames);
	// For a write or a message made now: its frame in the block being mixed
	// on the audio thread, 0 ("now") on any other
	int ahead() const;

private:
	uint32 _block = 0;
	int _frame = 0;
};

#endif
