// license:BSD-3-Clause
// copyright-holders:Nathan Woods, Curt Coder
/**********************************************************************

    MATRIX JKM6581 SID4

    SID with independent waveform from RAM, Independent filter,
    Stereo output, 8 voices, Mirroring mode

**********************************************************************/

#include "emu.h"
#include "jkm6581.h"
#include "sid4.h"



//**************************************************************************
//  MACROS / CONSTANTS
//**************************************************************************

#define LOG 0



//**************************************************************************
//  DEVICE DEFINITIONS
//**************************************************************************

// device type definition
DEFINE_DEVICE_TYPE(JKM6581, jkm6581_device, "jkm6581", "MATRIX JKM6581 SID4")



//**************************************************************************
//  LIVE DEVICE
//**************************************************************************

//-------------------------------------------------
//  jkm6581_device - constructor
//-------------------------------------------------

jkm6581_device::jkm6581_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock)
	: device_t(mconfig, JKM6581, tag, owner, clock)
	, device_sound_interface(mconfig, *this)
	, device_memory_interface(mconfig, *this)
	, m_data_config("data", ENDIANNESS_LITTLE, 8, 16)
	, m_stream(nullptr)
	, m_token(make_unique_clear<JKM6581_t>())
{
}

jkm6581_device::~jkm6581_device()
{
}

//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void jkm6581_device::device_start()
{
	m_data = &space(0);
	// Find our direct access
	m_cache = space(0).cache<0, 0, ENDIANNESS_LITTLE>();

	// create sound stream
	m_stream = machine().sound().stream_alloc(*this, 0, 2, machine().sample_rate());

	// initialize SID engine
	m_token->device = this;
	m_token->mixer_channel = m_stream;
	m_token->PCMfreq = machine().sample_rate();
	m_token->clock = clock();

	m_token->init(m_data, m_cache);
	for (int i = 0; i < maxLogicalVoices; i++)
	{
		save_item(NAME(m_token->optr[i].reg), i);
		save_item(NAME(m_token->optr[i].SIDfreq), i);
		save_item(NAME(m_token->optr[i].SIDctrl), i);
		save_item(NAME(m_token->optr[i].SIDAD), i);
		save_item(NAME(m_token->optr[i].SIDSR), i);
		save_item(NAME(m_token->optr[i].sync), i);
		save_item(NAME(m_token->optr[i].pulseIndex), i);
		save_item(NAME(m_token->optr[i].newPulseIndex), i);
		save_item(NAME(m_token->optr[i].SIDpulseWidth), i);
		save_item(NAME(m_token->optr[i].curSIDfreq), i);
		save_item(NAME(m_token->optr[i].curNoiseFreq), i);
		save_item(NAME(m_token->optr[i].output), i);
		save_item(NAME(m_token->optr[i].noiseOutput), i);
		save_item(NAME(m_token->optr[i].outputMask), i);
		save_item(NAME(m_token->optr[i].filtIO), i);
		save_item(NAME(m_token->optr[i].filtEnabled), i);
		save_item(NAME(m_token->optr[i].filtLow), i);
		save_item(NAME(m_token->optr[i].filtRef), i);
		save_item(NAME(m_token->optr[i].filter.Type), i);
		save_item(NAME(m_token->optr[i].filter.CurType), i);
		save_item(NAME(m_token->optr[i].filter.Value), i);
		save_item(NAME(m_token->optr[i].filter.Dy), i);
		save_item(NAME(m_token->optr[i].filter.ResDy), i);
		save_item(NAME(m_token->optr[i].cycleLenCount), i);
#if defined(DIRECT_FIXPOINT)
		save_item(NAME(m_token->optr[i].cycleLen.l), i);
		save_item(NAME(m_token->optr[i].cycleAddLen.l), i);
#else
		save_item(NAME(m_token->optr[i].cycleLen), i);
		save_item(NAME(m_token->optr[i].cycleLenPnt), i);
		save_item(NAME(m_token->optr[i].cycleAddLenPnt), i);
#endif
#if defined(DIRECT_FIXPOINT)
		save_item(NAME(m_token->optr[i].waveStepAdd.l), i);
		save_item(NAME(m_token->optr[i].waveStep.l), i);
		save_item(NAME(m_token->optr[i].wavePre[0].len), i);
		save_item(NAME(m_token->optr[i].wavePre[0].stp), i);
		save_item(NAME(m_token->optr[i].wavePre[1].len), i);
		save_item(NAME(m_token->optr[i].wavePre[1].stp), i);
#else
		save_item(NAME(m_token->optr[i].waveStepAdd), i);
		save_item(NAME(m_token->optr[i].waveStepAddPnt), i);
		save_item(NAME(m_token->optr[i].waveStep), i);
		save_item(NAME(m_token->optr[i].waveStepPnt), i);
		save_item(NAME(m_token->optr[i].wavePre[0].len), i);
		save_item(NAME(m_token->optr[i].wavePre[0].stp), i);
		save_item(NAME(m_token->optr[i].wavePre[0].pnt), i);
		save_item(NAME(m_token->optr[i].wavePre[1].len), i);
		save_item(NAME(m_token->optr[i].wavePre[1].stp), i);
		save_item(NAME(m_token->optr[i].wavePre[1].pnt), i);
#endif
		save_item(NAME(m_token->optr[i].waveStepOld), i);

#if defined(DIRECT_FIXPOINT)
		save_item(NAME(m_token->optr[i].noiseReg.l), i);
#else
		save_item(NAME(m_token->optr[i].noiseReg), i);
#endif
		save_item(NAME(m_token->optr[i].noiseStepAdd), i);
		save_item(NAME(m_token->optr[i].noiseStep), i);
		save_item(NAME(m_token->optr[i].noiseIsLocked), i);
		save_item(NAME(m_token->optr[i].ADSRctrl), i);
#ifdef SID_FPUENVE
		save_item(NAME(m_token->optr[i].fenveStep), i);
		save_item(NAME(m_token->optr[i].fenveStepAdd), i);
		save_item(NAME(m_token->optr[i].enveStep), i);
#elif defined(DIRECT_FIXPOINT)
		save_item(NAME(m_token->optr[i].enveStep.l), i);
		save_item(NAME(m_token->optr[i].enveStepAdd.l), i);
#else
		save_item(NAME(m_token->optr[i].enveStep), i);
		save_item(NAME(m_token->optr[i].enveStepAdd), i);
		save_item(NAME(m_token->optr[i].enveStepPnt), i);
		save_item(NAME(m_token->optr[i].enveStepAddPnt), i);
#endif
		save_item(NAME(m_token->optr[i].enveVol), i);
		save_item(NAME(m_token->optr[i].enveSusVol), i);
		save_item(NAME(m_token->optr[i].enveShortAttackCount), i);
	}
	sidInitWaveformTables();
}


//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void jkm6581_device::device_reset()
{
	m_token->reset();
}

//-------------------------------------------------
//  device_post_load - called after loading a saved state
//-------------------------------------------------

void jkm6581_device::device_post_load()
{
	m_token->postload();
}

//-------------------------------------------------
//  memory_space_config - return a description of
//  any address spaces owned by this device
//-------------------------------------------------

device_memory_interface::space_config_vector jkm6581_device::memory_space_config() const
{
	return space_config_vector{ std::make_pair(0, &m_data_config) };
}

//-------------------------------------------------
//  sound_stream_update - handle update requests for
//  our sound stream
//-------------------------------------------------

void jkm6581_device::sound_stream_update(sound_stream &stream, stream_sample_t **inputs, stream_sample_t **outputs, int samples)
{
	m_token->fill_buffer(outputs[0], outputs[1], samples);
}


//-------------------------------------------------
//  read -
//-------------------------------------------------

u8 jkm6581_device::read(offs_t offset)
{
	return m_token->port_r(machine(), offset);
}


//-------------------------------------------------
//  write -
//-------------------------------------------------

void jkm6581_device::write(offs_t offset, u8 data)
{
	m_token->port_w(offset, data);
}
