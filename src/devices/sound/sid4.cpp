// license:BSD-3-Clause
// copyright-holders:Peter Trauner
/*
  copyright peter trauner

  based on michael schwend's sid play

  Noise generation algorithm is used courtesy of Asger Alstrup Nielsen.
  His original publication can be found on the SID home page.

  Noise table optimization proposed by Phillip Wooller. The output of
  each table does not differ.
*/

#include "emu.h"
#include "sid4.h"

#include "sid4envel.h"


namespace {

const int mix16monoMiddleIndex = 256*maxLogicalVoices/2;
u16 mix16mono[256*maxLogicalVoices];

u16 zero16bit=0;  /* either signed or unsigned */
//u32 splitBufferLen;

void MixerInit(int threeVoiceAmplify)
{
	long si;
	u16 ui;
	long ampDiv = maxLogicalVoices;

	if (threeVoiceAmplify)
	{
		ampDiv = (maxLogicalVoices-1);
	}

	/* Mixing formulas are optimized by sample input value. */

	si = (-128*maxLogicalVoices) * 256;
	for (ui = 0; ui < sizeof(mix16mono)/sizeof(u16); ui++ )
	{
		mix16mono[ui] = (u16)(si/ampDiv) + zero16bit;
		si+=256;
	}

}

} // anonymous namespace


inline void JKM6581_t::syncEm()
{
	bool sync[maxLogicalVoices];
	for (int i = 0; i < maxLogicalVoices; i++)
	{
		sync[i] = optr[i].modulator->cycleLenCount <= 0;
	}

	for (int i = 0; i < maxLogicalVoices; i++)
	{
		optr[i].cycleLenCount--;
	}

	for (int i = 0; i < maxLogicalVoices; i++)
	{
		if (optr[i].sync && sync[i])
		{
			optr[i].cycleLenCount = 0;
			optr[i].outProc = &sid4Operator::wave_calc_normal;
#if defined(DIRECT_FIXPOINT)
			optr[i].waveStep.l = 0;
#else
			optr[i].waveStep = optr[i].waveStepPnt = 0;
#endif
		}
	}
}


void JKM6581_t::fill_buffer(stream_sample_t *lbuffer, stream_sample_t *rbuffer, u32 bufferLen)
{
//void* JKM6581_t::fill16bitMono(void* buffer, u32 numberOfSamples)

	for ( ; bufferLen > 0; bufferLen-- )
	{
		int lout = 0, rout = 0;

		for (int i = 0; i < maxLogicalVoices; i++)
		{
			lout += (((optr[i].outProc(&optr[i])&optr[i].outputMask)*optr[i].lvol) >> 8);
			rout += (((optr[i].outProc(&optr[i])&optr[i].outputMask)*optr[i].rvol) >> 8);
		}

		*lbuffer++ = (s16) mix16mono[unsigned(mix16monoMiddleIndex
								+lout
//                        +(*sampleEmuRout)()
		)];
		*rbuffer++ = (s16) mix16mono[unsigned(mix16monoMiddleIndex
								+rout
//                        +(*sampleEmuRout)()
		)];
		syncEm();
	}
}

/* --------------------------------------------------------------------- Init */


/* Reset. */

bool JKM6581_t::reset()
{
	for (int i = 0; i < maxLogicalVoices; i++)
	{
		optr[i].clear();
		enveEmuResetOperator( &optr[i] );
	}

	//sampleEmuReset();

	for (int i = 0; i < maxLogicalVoices; i++)
	{
		optr[i].set();
		optr[i].set2();
	}
	return true;
}

void JKM6581_t::postload()
{
	for (int i = 0; i < maxLogicalVoices; i++)
	{
		optr[i].set();
		optr[i].set2();
	}
}

void JKM6581_t::init(address_space *wave_space, memory_access_cache<0, 0, ENDIANNESS_LITTLE> *wave_cache)
{
	space = wave_space;
	cache = wave_cache;
	PCMsid = u32(PCMfreq * (16777216.0 / clock));
	PCMsidNoise = u32((clock * 256.0) / PCMfreq);

	for (int i = 0; i < maxLogicalVoices; i++)
	{
		optr[i].sid = this;

		optr[i].modulator = &optr[(i - 1) % maxLogicalVoices];
		optr[(i - 1) % maxLogicalVoices].carrier = &optr[i];

		optr[i].filter.Enabled = true;
		optr[i].filterTableInit(device->machine());
	}

	sidInitMixerEngine(device->machine());

	sidInitWaveformTables();

	enveEmuInit(PCMfreq, true);

	MixerInit(0);

	reset();
}


void JKM6581_t::port_w(int offset, int data)
{
	mixer_channel->update();
	if ((offset & 0xf) < 14)
	{
		optr[(offset >> 4) % maxLogicalVoices].reg[offset & 0xf] = data;
	}

	for (int i = 0; i < maxLogicalVoices; i++)
	{
		optr[i].set();

		// relies on sidEmuSet also for other channels!
		optr[i].set2();
	}
}


int JKM6581_t::port_r(running_machine &machine, int offset)
{
	return optr[(offset >> 4) % maxLogicalVoices].reg[offset & 0xf];
}
