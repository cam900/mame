// license:BSD-3-Clause
// copyright-holders:Bryan McPhail
/***************************************************************************

	MATRIX JKM052539

	K052539 with stereo output, 16 bit frequency

***************************************************************************/

#include "emu.h"
#include "jkm052539.h"
#include <algorithm>

#define FREQ_BITS   16
#define DEF_GAIN    8

void jkm052539_device::scc_map(address_map &map)
{
	map(0x00, 0x9f).rw(FUNC(jkm052539_device::jkm052539_waveform_r), FUNC(jkm052539_device::jkm052539_waveform_w));
	map(0xa0, 0xa9).mirror(0x10).w(FUNC(jkm052539_device::jkm052539_frequency_w));
	map(0xaa, 0xae).mirror(0x10).w(FUNC(jkm052539_device::jkm052539_volume_w));
	map(0xaf, 0xaf).mirror(0x10).w(FUNC(jkm052539_device::jkm052539_keyonoff_w));
	map(0xc0, 0xc0).mirror(0x1f).rw(FUNC(jkm052539_device::jkm052539_test_r), FUNC(jkm052539_device::jkm052539_test_w));
}

// device type definition
DEFINE_DEVICE_TYPE(JKM052539, jkm052539_device, "jkm052539", "JKM052539 SCC1")


//**************************************************************************
//  LIVE DEVICE
//**************************************************************************

//-------------------------------------------------
//  jkm052539_device - constructor
//-------------------------------------------------

jkm052539_device::jkm052539_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock)
	: device_t(mconfig, JKM052539, tag, owner, clock)
	, device_sound_interface(mconfig, *this)
	, m_stream(nullptr)
	, m_mclock(0)
	, m_rate(0)
	, m_mixer_table(nullptr)
	, m_mixer_lookup(nullptr)
	, m_test(0)
{
}


//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void jkm052539_device::device_start()
{
	// get stream channels
	m_rate = clock()/16;
	m_stream = stream_alloc(0, 2, m_rate);
	m_mclock = clock();

	// allocate a buffer to mix into - 1 second's worth should be more than enough
	m_mixer_buffer[0].resize(2 * m_rate);
	m_mixer_buffer[1].resize(2 * m_rate);

	// build the mixer table
	make_mixer_table(5);

	// save states
	for (int voice = 0; voice < 5; voice++)
	{
		save_item(NAME(m_channel_list[voice].counter), voice);
		save_item(NAME(m_channel_list[voice].frequency), voice);
		save_item(NAME(m_channel_list[voice].volume), voice);
		save_item(NAME(m_channel_list[voice].key), voice);
		save_item(NAME(m_channel_list[voice].waveram), voice);
	}
	save_item(NAME(m_test));
}


//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void jkm052539_device::device_reset()
{
	// reset all the voices
	for (sound_channel &voice : m_channel_list)
	{
		voice.frequency = 0;
		voice.volume = 0;
		voice.counter = 0;
		voice.key = false;
	}

	// other parameters
	m_test = 0;
}


//-------------------------------------------------
//  device_post_load - device-specific post-load
//-------------------------------------------------

void jkm052539_device::device_post_load()
{
	device_clock_changed();
}


//-------------------------------------------------
//  device_clock_changed - called if the clock
//  changes
//-------------------------------------------------

void jkm052539_device::device_clock_changed()
{
	const u32 old_rate = m_rate;
	m_rate = clock()/16;
	m_mclock = clock();

	if (old_rate < m_rate)
	{
		m_mixer_buffer[0].resize(2 * m_rate, 0);
		m_mixer_buffer[1].resize(2 * m_rate, 0);
	}
	m_stream->set_sample_rate(m_rate);
}


//-------------------------------------------------
//  sound_stream_update - handle a stream update
//-------------------------------------------------

void jkm052539_device::sound_stream_update(sound_stream &stream, stream_sample_t **inputs, stream_sample_t **outputs, int samples)
{
	// zap the contents of the mixer buffer
	std::fill(m_mixer_buffer[0].begin(), m_mixer_buffer[0].end(), 0);
	std::fill(m_mixer_buffer[1].begin(), m_mixer_buffer[1].end(), 0);

	for (sound_channel &voice : m_channel_list)
	{
		// channel is halted for freq <= 0
		if (voice.frequency > 0)
		{
			const int v = voice.volume * voice.key;
			int c = voice.counter;
			const int step = ((s64(m_mclock) << FREQ_BITS) / float((voice.frequency + 1) * 16 * (m_rate / 32))) + 0.5f;

			// add our contribution
			for (int i = 0; i < samples; i++)
			{
				c += step;
				const int offs = (c >> FREQ_BITS) & 0x1f;
				m_mixer_buffer[0][i] += (voice.waveram[offs] * (v & 0xf)) >> 3;
				m_mixer_buffer[1][i] += (voice.waveram[offs] * ((v >> 4) & 0xf)) >> 3;
			}

			// update the counter for this voice
			voice.counter = c;
		}
	}

	// mix it down
	stream_sample_t *buffer_l = outputs[0];
	stream_sample_t *buffer_r = outputs[1];
	for (int i = 0; i < samples; i++)
	{
		*buffer_l++ = m_mixer_lookup[m_mixer_buffer[0][i]];
		*buffer_r++ = m_mixer_lookup[m_mixer_buffer[1][i]];
	}
}


/********************************************************************************/


void jkm052539_device::jkm052539_waveform_w(offs_t offset, u8 data)
{
	// waveram is read-only?
	if (m_test & 0x40)
		return;

	m_stream->update();
	m_channel_list[offset >> 5].waveram[offset & 0x1f] = data;
}


u8 jkm052539_device::jkm052539_waveform_r(offs_t offset)
{
	// test-register bit 6 exposes the internal counter
	if (m_test & 0x40)
	{
		m_stream->update();
		offset += (m_channel_list[offset >> 5].counter >> FREQ_BITS);
	}
	return m_channel_list[offset >> 5].waveram[offset & 0x1f];
}


void jkm052539_device::jkm052539_volume_w(offs_t offset, u8 data)
{
	m_stream->update();
	m_channel_list[offset & 0x7].volume = data & 0xff;
}


void jkm052539_device::jkm052539_frequency_w(offs_t offset, u8 data)
{
	const int freq_hi = offset & 1;
	offset >>= 1;

	m_stream->update();

	// test-register bit 5 resets the internal counter
	if (m_test & 0x20)
		m_channel_list[offset].counter = ~0;
	else if (m_channel_list[offset].frequency <= 0)
		m_channel_list[offset].counter |= ((1 << FREQ_BITS) - 1);

	// update frequency
	if (freq_hi)
		m_channel_list[offset].frequency = (m_channel_list[offset].frequency & 0x00ff) | (data << 8 & 0xff00);
	else
		m_channel_list[offset].frequency = (m_channel_list[offset].frequency & 0xff00) | data;
}


void jkm052539_device::jkm052539_keyonoff_w(u8 data)
{
	m_stream->update();

	for (int i = 0; i < 5; i++)
	{
		m_channel_list[i].key = BIT(data, i);
	}
}


void jkm052539_device::jkm052539_test_w(u8 data)
{
	m_test = data;
}


u8 jkm052539_device::jkm052539_test_r()
{
	// reading the test register sets it to $ff!
	if (!machine().side_effects_disabled())
		jkm052539_test_w(0xff);
	return 0xff;
}


//-------------------------------------------------
// build a table to divide by the number of voices
//-------------------------------------------------

void jkm052539_device::make_mixer_table(int voices)
{
	// allocate memory
	m_mixer_table = std::make_unique<s16[]>(512 * voices);

	// find the middle of the table
	m_mixer_lookup = m_mixer_table.get() + (256 * voices);

	// fill in the table - 16 bit case
	for (int i = 0; i < (voices * 256); i++)
	{
		const int val = std::min(32767, i * DEF_GAIN * 16 / voices);
		m_mixer_lookup[ i] = val;
		m_mixer_lookup[-i] = -val;
	}
}
