// license:BSD-3-Clause
// copyright-holders:Nathan Woods, Curt Coder
/**********************************************************************

    MATRIX JKM8580 Sound Interface Device emulation

**********************************************************************/

#include "emu.h"
#include "jkm8580.h"
#include "sid2.h"



//**************************************************************************
//  MACROS / CONSTANTS
//**************************************************************************

#define LOG 0



//**************************************************************************
//  DEVICE DEFINITIONS
//**************************************************************************

// device type definition
DEFINE_DEVICE_TYPE(JKM8580, jkm8580_device, "jkm8580", "MATRIX JKM8580 SID2")



//**************************************************************************
//  LIVE DEVICE
//**************************************************************************

//-------------------------------------------------
//  jkm8580_device - constructor
//-------------------------------------------------

jkm8580_device::jkm8580_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, JKM8580, tag, owner, clock)
	, device_sound_interface(mconfig, *this)
	, m_stream(nullptr)
	, m_token(make_unique_clear<JKM8580_t>())

{
}

jkm8580_device::~jkm8580_device()
{
}

//-------------------------------------------------
//  save_state - add save states
//-------------------------------------------------

void jkm8580_device::save_state(JKM8580_t *token)
{
	save_item(NAME(token->clock));

	save_item(NAME(token->PCMfreq));
	save_item(NAME(token->PCMsid));
	save_item(NAME(token->PCMsidNoise));

	//save_item(NAME(token->sidKeysOn));
	//save_item(NAME(token->sidKeysOff));

	for (int v = 0; v < m_token->max_voices; v++)
	{
		save_item(NAME(token->optr[v].reg), v);

		save_item(NAME(token->optr[v].SIDfreq), v);
		save_item(NAME(token->optr[v].SIDpulseWidth), v);
		save_item(NAME(token->optr[v].SIDctrl), v);
		save_item(NAME(token->optr[v].SIDAD), v);
		save_item(NAME(token->optr[v].SIDSR), v);

		save_item(NAME(token->optr[v].sync), v);

		save_item(NAME(token->optr[v].pulseIndex), v);
		save_item(NAME(token->optr[v].newPulseIndex), v);

		save_item(NAME(token->optr[v].curSIDfreq), v);
		save_item(NAME(token->optr[v].curNoiseFreq), v);

		save_item(NAME(token->optr[v].output), v);
		//save_item(NAME(token->optr[v].outputMask), v);

		save_item(NAME(token->optr[v].masterVolume), v);
		save_item(NAME(token->optr[v].masterVolumeAmplIndex), v);

		save_item(NAME(token->optr[v].filter.Enabled), v);
		save_item(NAME(token->optr[v].filter.Type), v);
		save_item(NAME(token->optr[v].filter.CurType), v);
		save_item(NAME(token->optr[v].filter.Dy), v);
		save_item(NAME(token->optr[v].filter.ResDy), v);
		save_item(NAME(token->optr[v].filter.Value), v);

		save_item(NAME(token->optr[v].filtEnabled), v);
		save_item(NAME(token->optr[v].filtLow), v);
		save_item(NAME(token->optr[v].filtRef), v);
		save_item(NAME(token->optr[v].filtIO), v);

		save_item(NAME(token->optr[v].cycleLenCount), v);
#if defined(DIRECT_FIXPOINT)
		save_item(NAME(token->optr[v].cycleLen.l), v);
		save_item(NAME(token->optr[v].cycleAddLen.l), v);
#else
		save_item(NAME(token->optr[v].cycleAddLenPnt), v);
		save_item(NAME(token->optr[v].cycleLen), v);
		save_item(NAME(token->optr[v].cycleLenPnt), v);
#endif

#if defined(DIRECT_FIXPOINT)
		save_item(NAME(token->optr[v].waveStep.l), v);
		save_item(NAME(token->optr[v].waveStepAdd.l), v);
#else
		save_item(NAME(token->optr[v].waveStep), v);
		save_item(NAME(token->optr[v].waveStepAdd), v);
		save_item(NAME(token->optr[v].waveStepPnt), v);
		save_item(NAME(token->optr[v].waveStepAddPnt), v);
#endif
		save_item(NAME(token->optr[v].waveStepOld), v);
		for (int n = 0; n < 2; n++)
		{
			save_item(NAME(token->optr[v].wavePre[n].len), v | (n << 4));
#if defined(DIRECT_FIXPOINT)
			save_item(NAME(token->optr[v].wavePre[n].stp), v | (n << 4));
#else
			save_item(NAME(token->optr[v].wavePre[n].pnt), v | (n << 4));
			save_item(NAME(token->optr[v].wavePre[n].stp), v | (n << 4));
#endif
		}

#if defined(DIRECT_FIXPOINT)
		save_item(NAME(token->optr[v].noiseReg.l), v);
#else
		save_item(NAME(token->optr[v].noiseReg), v);
#endif
		save_item(NAME(token->optr[v].noiseStep), v);
		save_item(NAME(token->optr[v].noiseStepAdd), v);
		save_item(NAME(token->optr[v].noiseOutput), v);
		save_item(NAME(token->optr[v].noiseIsLocked), v);

		save_item(NAME(token->optr[v].ADSRctrl), v);
		//save_item(NAME(token->optr[v].gateOnCtrl), v);
		//save_item(NAME(token->optr[v].gateOffCtrl), v);

#ifdef SID_FPUENVE
		save_item(NAME(token->optr[v].fenveStep), v);
		save_item(NAME(token->optr[v].fenveStepAdd), v);
		save_item(NAME(token->optr[v].enveStep), v);
#elif defined(DIRECT_FIXPOINT)
		save_item(NAME(token->optr[v].enveStep.l), v);
		save_item(NAME(token->optr[v].enveStepAdd.l), v);
#else
		save_item(NAME(token->optr[v].enveStep), v);
		save_item(NAME(token->optr[v].enveStepAdd), v);
		save_item(NAME(token->optr[v].enveStepPnt), v);
		save_item(NAME(token->optr[v].enveStepAddPnt), v);
#endif
		save_item(NAME(token->optr[v].enveVol), v);
		save_item(NAME(token->optr[v].enveSusVol), v);
		save_item(NAME(token->optr[v].enveShortAttackCount), v);
	}
}

//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void jkm8580_device::device_start()
{
	// create sound stream
	m_stream = machine().sound().stream_alloc(*this, 0, 2, machine().sample_rate());

	// initialize SID engine
	m_token->device = this;
	m_token->mixer_channel = m_stream;
	m_token->PCMfreq = machine().sample_rate();
	m_token->clock = clock();

	m_token->init();
	sid2InitWaveformTables();
	save_state(m_token.get());
}


//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void jkm8580_device::device_reset()
{
	m_token->reset();
}


//-------------------------------------------------
//  device_post_load - device-specific post-load
//-------------------------------------------------

void jkm8580_device::device_post_load()
{
	m_token->postload();
}


//-------------------------------------------------
//  sound_stream_update - handle update requests for
//  our sound stream
//-------------------------------------------------

void jkm8580_device::sound_stream_update(sound_stream &stream, stream_sample_t **inputs, stream_sample_t **outputs, int samples)
{
	m_token->fill_buffer(outputs[0], outputs[1], samples);
}


//-------------------------------------------------
//  read -
//-------------------------------------------------

uint8_t jkm8580_device::read(offs_t offset)
{
	return m_token->port_r(machine(), offset);
}


//-------------------------------------------------
//  write -
//-------------------------------------------------

void jkm8580_device::write(offs_t offset, uint8_t data)
{
	m_token->port_w(offset, data);
}
