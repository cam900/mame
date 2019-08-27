// license:BSD-3-Clause
// copyright-holders:Bryan McPhail
#ifndef MAME_SOUND_JKM052539_H
#define MAME_SOUND_JKM052539_H

#pragma once


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

// ======================> jkm052539_device

class jkm052539_device : public device_t,
						public device_sound_interface
{
public:
	jkm052539_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock);

	void jkm052539_waveform_w(offs_t offset, u8 data);
	u8   jkm052539_waveform_r(offs_t offset);
	void jkm052539_volume_w(offs_t offset, u8 data);
	void jkm052539_frequency_w(offs_t offset, u8 data);
	void jkm052539_keyonoff_w(u8 data);
	void jkm052539_test_w(u8 data);
	u8   jkm052539_test_r();

	void scc_map(address_map &map);
protected:
	// device-level overrides
	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void device_post_load() override;
	virtual void device_clock_changed() override;

	// sound stream update overrides
	virtual void sound_stream_update(sound_stream &stream, stream_sample_t **inputs, stream_sample_t **outputs, int samples) override;

private:
	// Parameters for a channel
	struct sound_channel
	{
		sound_channel() :
			counter(0),
			frequency(0),
			volume(0),
			key(false)
		{
			std::fill(std::begin(waveram), std::end(waveram), 0);
		}

		u64 counter;
		int frequency;
		int volume;
		bool key;
		s8 waveram[32];
	};

	void make_mixer_table(int voices);

	sound_channel m_channel_list[5];

	/* global sound parameters */
	sound_stream *m_stream;
	int m_mclock;
	int m_rate;

	/* mixer tables and internal buffers */
	std::unique_ptr<s16[]> m_mixer_table;
	s16 *m_mixer_lookup;
	std::vector<s16> m_mixer_buffer[2];

	/* chip registers */
	u8 m_test;
};

DECLARE_DEVICE_TYPE(JKM052539, jkm052539_device)

#endif // MAME_SOUND_JKM052539_H
