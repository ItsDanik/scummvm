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

#include "backends/platform/sdl/mister/mister-mixer.h"

#include <mister_hybrid.h>
#include <pthread.h>

// The block the audio thread is mixing: counted up as each starts
static volatile uint32 mixBlock = 0;
static volatile bool mixing = false;
static pthread_t mixThread;

void MisterMixerManager::init() {
	SdlMixerManager::init();
	// the MIDI synthesizer: its SoundFont starts loading in a thread now,
	// and is there by the time a game plays music (hybrid/README.md, "MIDI
	// and the MT32-pi"); needs the audio, which SDL's driver has enabled
	if (_isAudioOpen)
		MH_MidiEnable(1);
}

void MisterMixerManager::callbackHandler(byte *samples, int len) {
	mixThread = pthread_self();
	mixBlock = mixBlock + 1;
	mixing = true;
	SdlMixerManager::callbackHandler(samples, len);
	mixing = false;
}

void MisterAudioClock::advance(int frames) {
	if (_block != mixBlock) {
		_block = mixBlock;
		_frame = 0;
	}
	_frame += frames;
}

int MisterAudioClock::ahead() const {
	if (!mixing || !pthread_equal(pthread_self(), mixThread) || _block != mixBlock)
		return 0;
	return _frame;
}
