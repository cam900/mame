// license:BSD-3-Clause
// copyright-holders:Peter Trauner
#ifndef MAME_SOUND_SID2_H
#define MAME_SOUND_SID2_H

#pragma once

/*
  approximation of the sid6581 chip
  this part is for one chip,
*/

#include "sid2voice.h"

/* private area */
struct JKM8580_t
{
	static constexpr uint8_t max_voices = 8;

	device_t *device;
	sound_stream *mixer_channel; // mame stream/ mixer channel

	uint32_t clock;

	uint16_t PCMfreq; // samplerate of the current systems soundcard/DAC
	uint32_t PCMsid, PCMsidNoise;

//  bool sidKeysOn[0x20], sidKeysOff[0x20];

	jkm8580Operator optr[max_voices];

	void init();

	bool reset();

	void postload();

	int port_r(running_machine &machine, int offset);
	void port_w(int offset, int data);

	void fill_buffer(stream_sample_t *lbuffer, stream_sample_t *rbuffer, uint32_t bufferLen);

private:
	void syncEm();
};

#endif // MAME_SOUND_SID2_H
