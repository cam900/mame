// license:BSD-3-Clause
// copyright-holders:Peter Trauner
#ifndef MAME_SOUND_SID4_H
#define MAME_SOUND_SID4_H

#pragma once

/*
  approximation of the jkm6581 chip
  this part is for one chip,
*/

#define maxLogicalVoices 8

#include "sid4voice.h"

/* private area */
struct JKM6581_t
{
	address_space                                *space;
	memory_access_cache<0, 0, ENDIANNESS_LITTLE> *cache;
	device_t *device;
	sound_stream *mixer_channel; // mame stream/ mixer channel

	u32 clock;

	u16 PCMfreq; // samplerate of the current systems soundcard/DAC
	u32 PCMsid, PCMsidNoise;

//  bool sidKeysOn[0x20], sidKeysOff[0x20];

	u8 masterVolume;
	u16 masterVolumeAmplIndex;

	sid4Operator optr[maxLogicalVoices];
	int optr3_outputmask;

	void init(address_space *wave_space, memory_access_cache<0, 0, ENDIANNESS_LITTLE> *wave_cache);

	bool reset();
	void postload();

	int port_r(running_machine &machine, int offset);
	void port_w(int offset, int data);

	void fill_buffer(stream_sample_t *lbuffer, stream_sample_t *rbuffer, u32 bufferLen);

private:
	void syncEm();
};

#endif // MAME_SOUND_SID4_H
