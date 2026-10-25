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

#include "audio/musicplugin.h"
#include "audio/softsynth/emumidi.h"
#include "backends/platform/sdl/mister/mister-mixer.h"
#include "common/array.h"
#include "common/error.h"
#include "common/mutex.h"
#include "common/system.h"

#include <mister_hybrid.h>
#include <string.h>

// MIDI music played outside the ARM (hybrid/README.md, "MIDI and the
// MT32-pi"): on the core's SoundFont synthesizer, or on an MT32-pi on the
// user port, whichever the OSD says. To the engines a General MIDI device
// (with native_mt32 set while the MT32-pi plays as an MT-32: the games' own
// MT-32 music then goes to it).
//
// It is a silent stream in the mixer, as a software synthesizer would be: the
// music's timer runs in it, so every message is made at its audio frame
// (MH_MidiEvent()'s "ahead"). It keeps what the song has set (programs,
// controllers, pitch bend and its range, the system exclusive messages) and
// sends it again when the synthesizer lost it (MH_MidiSynthChanged(): the
// music moved between the MT32-pi and the core, or the Pi switched synth),
// holding back what follows such a burst until it has gone out on the Pi's
// cable (MH_MidiBacklog()).
class MidiDriver_MiSTer : public MidiDriver_Emulated {
public:
	MidiDriver_MiSTer(Audio::Mixer *mixer) : MidiDriver_Emulated(mixer) {}
	~MidiDriver_MiSTer() override { close(); }

	int open() override;
	void close() override;
	void send(uint32 b) override;
	void sysEx(const byte *msg, uint16 length) override;

	MidiChannel *allocateChannel() override { return nullptr; }
	MidiChannel *getPercussionChannel() override { return nullptr; }

	bool isStereo() const override { return false; }
	int getRate() const override { return _mixer->getOutputRate(); }

protected:
	void generateSamples(int16 *buf, int len) override;

private:
	enum { kUnset = -1, kSysExLog = 65536 };

	struct Channel {
		int16 cc[128];
		int16 program;
		int16 bendLsb, bendMsb;
		int16 pressure;
		int16 rpnLsb, rpnMsb;
		int16 rangeMsb, rangeLsb; // pitch bend range (RPN 0)
	};

	int ahead() const;
	void event(int status, int data1, int data2);
	void record(int status, int data1, int data2);
	void resetState();
	void sendState();
	void hold();

	Common::Mutex _mutex;
	MisterAudioClock _clock;
	uint64 _frames = 0;     // made since open(), the frame of a message now
	uint64 _holdUntil = 0;  // messages wait for this frame (a burst on the Pi's cable)
	Channel _channels[16];
	Common::Array<byte> _sysex; // length (2 bytes) and message, for each
};

int MidiDriver_MiSTer::open() {
	if (_isOpen)
		return MERR_ALREADY_OPEN;
	MidiDriver_Emulated::open();
	resetState();
	MH_MidiVolume(32);
	// the synthesizer the core ran before kept what its game set
	static const byte gmReset[] = { 0x7E, 0x7F, 0x09, 0x01 };
	sysEx(gmReset, sizeof(gmReset));
	_mixer->playStream(Audio::Mixer::kPlainSoundType, &_mixerSoundHandle, this, -1,
	                   Audio::Mixer::kMaxChannelVolume, 0, DisposeAfterUse::NO, true);
	return 0;
}

void MidiDriver_MiSTer::close() {
	if (!_isOpen)
		return;
	_isOpen = false;
	_mixer->stopHandle(_mixerSoundHandle);
	Common::StackLock lock(_mutex);
	MH_MidiReset(0);
}

// Its frame in the block being mixed, later while a burst goes out. (On the
// audio thread _frames counts the block's frames made so far: it is now.)
int MidiDriver_MiSTer::ahead() const {
	int frame = _clock.ahead();

	if (_holdUntil > _frames)
		frame += (int)(_holdUntil - _frames);
	return frame;
}

void MidiDriver_MiSTer::event(int status, int data1, int data2) {
	MH_MidiEvent(ahead(), status, data1, data2);
}

void MidiDriver_MiSTer::send(uint32 b) {
	const int status = b & 0xFF;
	const int data1 = (b >> 8) & 0x7F;
	const int data2 = (b >> 16) & 0x7F;

	if (status < 0x80 || status >= 0xF0)
		return;
	Common::StackLock lock(_mutex);
	record(status, data1, data2);
	event(status, data1, data2);
}

void MidiDriver_MiSTer::sysEx(const byte *msg, uint16 length) {
	Common::StackLock lock(_mutex);
	// a General MIDI or GS reset starts the song's settings anew
	const bool reset = (length >= 4 && msg[0] == 0x7E && msg[2] == 0x09) ||
	                   (length >= 9 && msg[0] == 0x41 && msg[2] == 0x42 && msg[3] == 0x12 && msg[4] == 0x40 && msg[5] == 0x00 && msg[6] == 0x7F);

	if (reset) {
		resetState();
		_sysex.clear();
	}
	if (_sysex.size() + length + 2 <= kSysExLog) {
		_sysex.push_back(length & 0xFF);
		_sysex.push_back(length >> 8);
		const uint size = _sysex.size();
		_sysex.resize(size + length);
		memcpy(_sysex.data() + size, msg, length);
	}
	MH_MidiSysEx(ahead(), msg, length);
	if (reset)
		hold();
}

void MidiDriver_MiSTer::record(int status, int data1, int data2) {
	Channel &c = _channels[status & 0x0F];

	switch (status & 0xF0) {
	case 0xB0:
		if (data1 == 121) {
			// Reset All Controllers: not bank, volume, pan and the effects
			for (int i = 1; i < 120; i++) {
				if (i != 7 && i != 10 && i != 32 && i != 91 && i != 93)
					c.cc[i] = kUnset;
			}
			c.bendLsb = c.bendMsb = c.pressure = kUnset;
			c.rpnLsb = c.rpnMsb = kUnset;
		} else if (data1 < 120) {
			c.cc[data1] = data2;
			if (data1 == 101)
				c.rpnMsb = data2;
			else if (data1 == 100)
				c.rpnLsb = data2;
			else if (data1 == 6 && c.rpnMsb == 0 && c.rpnLsb == 0)
				c.rangeMsb = data2;
			else if (data1 == 38 && c.rpnMsb == 0 && c.rpnLsb == 0)
				c.rangeLsb = data2;
		}
		break;
	case 0xC0:
		c.program = data1;
		break;
	case 0xD0:
		c.pressure = data1;
		break;
	case 0xE0:
		c.bendLsb = data1;
		c.bendMsb = data2;
		break;
	default:
		break;
	}
}

void MidiDriver_MiSTer::resetState() {
	for (int ch = 0; ch < 16; ch++) {
		Channel &c = _channels[ch];

		for (int i = 0; i < 128; i++)
			c.cc[i] = kUnset;
		c.program = c.bendLsb = c.bendMsb = c.pressure = kUnset;
		c.rpnLsb = c.rpnMsb = c.rangeMsb = c.rangeLsb = kUnset;
	}
}

// What the song has set, again: its system exclusive messages first (a reset
// among them would undo the rest), then each channel's bank and program, its
// controllers, pitch bend range, pitch bend and pressure
void MidiDriver_MiSTer::sendState() {
	for (uint i = 0; i + 2 <= _sysex.size();) {
		const uint length = _sysex[i] | (_sysex[i + 1] << 8);

		MH_MidiSysEx(ahead(), &_sysex[i + 2], length);
		i += 2 + length;
	}
	for (int ch = 0; ch < 16; ch++) {
		const Channel &c = _channels[ch];

		if (c.cc[0] != kUnset)
			event(0xB0 | ch, 0, c.cc[0]);
		if (c.cc[32] != kUnset)
			event(0xB0 | ch, 32, c.cc[32]);
		if (c.program != kUnset)
			event(0xC0 | ch, c.program, 0);
		for (int i = 1; i < 120; i++) {
			// the bank above, data entry and the parameter numbers below
			if (c.cc[i] == kUnset || i == 6 || i == 32 || i == 38 || (i >= 96 && i <= 101))
				continue;
			event(0xB0 | ch, i, c.cc[i]);
		}
		if (c.rangeMsb != kUnset) {
			event(0xB0 | ch, 101, 0);
			event(0xB0 | ch, 100, 0);
			event(0xB0 | ch, 6, c.rangeMsb);
			if (c.rangeLsb != kUnset)
				event(0xB0 | ch, 38, c.rangeLsb);
			event(0xB0 | ch, 101, 127);
			event(0xB0 | ch, 100, 127);
		}
		if (c.bendMsb != kUnset)
			event(0xE0 | ch, c.bendLsb, c.bendMsb);
		if (c.pressure != kUnset)
			event(0xD0 | ch, c.pressure, 0);
	}
}

// What comes next waits until a burst sent now has gone out on the MT32-pi's
// cable (0 frames for the synthesizer in the core)
void MidiDriver_MiSTer::hold() {
	_holdUntil = _frames + MH_MidiBacklog();
}

void MidiDriver_MiSTer::generateSamples(int16 *buf, int len) {
	memset(buf, 0, len * sizeof(int16));
	Common::StackLock lock(_mutex);
	// the synthesizer lost the song's settings since the last block
	if (MH_MidiSynthChanged() && MH_MidiWanted()) {
		sendState();
		hold();
	}
	_clock.advance(len);
	_frames += len;
}

class MisterMusicPlugin : public MusicPluginObject {
public:
	const char *getName() const override {
		return "MiSTer MIDI";
	}

	const char *getId() const override {
		return "mister";
	}

	MusicDevices getDevices() const override {
		MusicDevices devices;
		devices.push_back(MusicDevice(this, "", MT_GM));
		return devices;
	}

	Common::Error createInstance(MidiDriver **mididriver, MidiDriver::DeviceHandle = 0) const override {
		*mididriver = new MidiDriver_MiSTer(g_system->getMixer());
		return Common::kNoError;
	}
};

//#if PLUGIN_ENABLED_DYNAMIC(MISTER)
	//REGISTER_PLUGIN_DYNAMIC(MISTER, PLUGIN_TYPE_MUSIC, MisterMusicPlugin);
//#else
	REGISTER_PLUGIN_STATIC(MISTER, PLUGIN_TYPE_MUSIC, MisterMusicPlugin);
//#endif
