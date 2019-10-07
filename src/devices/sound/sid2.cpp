// license:BSD-3-Clause
// copyright-holders:Peter Trauner
/*
  copyright peter trauner

  based on michael schwend's sid play

  Noise generation algorithm is used courtesy of Asger Alstrup Nielsen.
  His original publication can be found on the SID home page.

  Noise table optimization proposed by Phillip Wooller. The output of
  each table does not differ.

  MOS-8580 R5 combined waveforms recorded by Dennis "Deadman" Lindroos.
*/

#include "emu.h"
#include "sid2.h"

#include "sid2envel.h"


namespace {

#define maxLogicalVoices 8

const int mix16monoMiddleIndex = 256*maxLogicalVoices/2;
uint16_t mix16mono[256*maxLogicalVoices];

uint16_t zero16bit = 0;  /* either signed or unsigned */
//uint32_t splitBufferLen;

void MixerInit(int threeVoiceAmplify)
{
	long si;
	uint16_t ui;
	long ampDiv = maxLogicalVoices;

	if (threeVoiceAmplify)
	{
		ampDiv = (maxLogicalVoices-1);
	}

	/* Mixing formulas are optimized by sample input value. */

	si = (-128*maxLogicalVoices) * 256;
	for (ui = 0; ui < sizeof(mix16mono)/sizeof(uint16_t); ui++)
	{
		mix16mono[ui] = (uint16_t)(si/ampDiv) + zero16bit;
		si+=256;
	}

}

} // anonymous namespace


inline void JKM8580_t::syncEm()
{
	bool sync[3];
	for (int v = 0; v < max_voices; v++)
	{
		sync[v] = optr[v].modulator->cycleLenCount <= 0;
		optr[v].cycleLenCount--;
	}

	for (int v = 0; v < max_voices; v++)
	{
		if (optr[v].sync && sync[v])
		{
			optr[v].cycleLenCount = 0;
			optr[v].outProc = &jkm8580Operator::wave_calc_normal;
#if defined(DIRECT_FIXPOINT)
			optr[v].waveStep.l = 0;
#else
			optr[v].waveStep = optr[v].waveStepPnt = 0;
#endif
		}
	}
}


void JKM8580_t::fill_buffer(stream_sample_t *lbuffer, stream_sample_t *rbuffer, uint32_t bufferLen)
{
//void* JKM8580_t::fill16bitMono(void* buffer, uint32_t numberOfSamples)

	int output;
	for (; bufferLen > 0; bufferLen--)
	{
		output = 0;
		for (int v = 0; v < max_voices; v++)
		{
			output += ((optr[v].outProc(&optr[v])&optr[v].outputLMask) * optr[v].reg[12]) / 0xff;
		}

		*lbuffer++ = (int16_t) mix16mono[unsigned(mix16monoMiddleIndex
								+output
//                        +(*sampleEmuRout)()
		)];
		output = 0;
		for (int v = 0; v < max_voices; v++)
		{
			output += ((optr[v].outProc(&optr[v])&optr[v].outputRMask) * optr[v].reg[13]) / 0xff;
		}

		*rbuffer++ = (int16_t) mix16mono[unsigned(mix16monoMiddleIndex
								+output
//                        +(*sampleEmuRout)()
		)];
		syncEm();
	}
}

/* --------------------------------------------------------------------- Init */


/* Reset. */

bool JKM8580_t::reset()
{
	for (int v = 0; v < max_voices; v++)
	{
		optr[v].clear();
		enveEmuResetOperator(&optr[v]);
	}

	//sampleEmuReset();

	for (int v = 0; v < max_voices; v++)
	{
		optr[v].set();
		optr[v].set2();
	}

	return true;
}

void JKM8580_t::postload()
{
	for (int v = 0; v < max_voices; v++)
	{
		optr[v].set();
		optr[v].set2();
	}
}


void JKM8580_t::init()
{
	for (int v = 0; v < max_voices; v++)
	{
		optr[v].sid = this;

		int mod_voi = (v - 1) % max_voices;
		optr[v].modulator = &optr[mod_voi];
		optr[mod_voi].carrier = &optr[v];
	}

	PCMsid = uint32_t(PCMfreq * (16777216.0 / clock));
	PCMsidNoise = uint32_t((clock * 256.0) / PCMfreq);

	sidInitMixerEngine(device->machine());
	filterTableInit(device->machine());

	sid2InitWaveformTables();

	enveEmuInit(PCMfreq, true);

	MixerInit(0);

	reset();
}


void JKM8580_t::port_w(int offset, int data)
{
	if (offset & 0xf < 14)
	{
		mixer_channel->update();
		optr[(offset >> 4) % max_voices].reg[offset & 0xf] = data;

		for (int v = 0; v < max_voices; v++)
		{
			optr[v].set();
			// relies on sidEmuSet also for other channels!
			optr[v].set2();
		}
	}
}


int JKM8580_t::port_r(running_machine &machine, int offset)
{
	return optr[(offset >> 4) % max_voices].reg[offset & 0xf];
}
