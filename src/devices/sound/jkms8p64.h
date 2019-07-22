
// license:BSD-3-Clause
// copyright-holders:MATRIX
/*
	MATRIX JKMS8P64
	8 Channel PCM/ADPCM Player
*/
#ifndef MAME_SOUND_JKMS8P64_H
#define MAME_SOUND_JKMS8P64_H

#pragma once


class jkms8p64_device : public device_t, public device_sound_interface, public device_memory_interface
{
public:
	jkms8p64_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	u32 dword_r(offs_t offset);
	void dword_w(offs_t offset, u32 data, u32 mem_mask = ~0);

	u16 word_be_r(offs_t offset);
	void word_be_w(offs_t offset, u16 data, u16 mem_mask = ~0);

	u16 word_le_r(offs_t offset);
	void word_le_w(offs_t offset, u16 data, u16 mem_mask = ~0);

	u8 byte_be_r(offs_t offset);
	void byte_be_w(offs_t offset, u8 data, u8 mem_mask = ~0);

	u8 byte_le_r(offs_t offset);
	void byte_le_w(offs_t offset, u8 data, u8 mem_mask = ~0);

protected:
	virtual void sound_stream_update(sound_stream &stream, stream_sample_t **inputs, stream_sample_t **outputs, int samples) override;

	// device-level overrides
	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void device_clock_changed() override;

	// device_memory_interface overrides
	virtual space_config_vector memory_space_config() const override;

private:
	static constexpr u8 MAX_CHANNEL = 64;

	struct yadpcm_t
	{
		s32 step = 0x7f;
		s32 signal = 0;
		s32 loopstep = 0x7f;
		s32 loopsignal = 0;
		bool looped = false;

		void reset()
		{
			step = 0x7f;
			signal = 0;
			loopstep = 0x7f;
			loopsignal = 0;
			looped = false;
		}

		void save_loop()
		{
			if (!looped)
			{
				loopstep = step;
				loopsignal = loopsignal;
				looped = true;
			}
		}

		void load_loop()
		{
			if (looped)
			{
				step = loopstep;
				signal = loopsignal;
			}
		}

		s32 update(u8 nibble)
		{
			static constexpr int diff_lookup[16] = {1, 3, 5, 7, 9, 11, 13, 15, -1, -3, -5, -7, -9, -11, -13, -15};
			static constexpr int index_scale[8] = { 0x0e6, 0x0e6, 0x0e6, 0x0e6, 0x133, 0x199, 0x200, 0x266 };

			signal = std::clamp(signal + (step * diff_lookup[nibble & 15]) / 8, -32768, 32767);

			/* adjust the step size and clamp */
			step = std::clamp((step * index_scale[nibble & 7]) >> 8, 0x7f, 0x6000);
		}
	};

	struct channel_t
	{
		bool playing = false;
		bool looping = false;
		u32 ctrl = 0;
		u32 freq = 0;
		u64 frac = 0;
		u64 update = 0;
		u32 start = 0;
		u32 loop = 0;
		u32 loopend = 0;
		u32 end = 0;
		u32 pos = 0;
		u16 mvol = 0;
		u16 lvol = 0;
		u16 rvol = 0;
		s32 prev_sample[2] = {0, 0};
		s32 curr_sample[2] = {0, 0};
		yadpcm_t adpcm[2];

		void keyon()
		{
			std::fill(std::begin(prev_sample), std::end(prev_sample), 0);
			std::fill(std::begin(curr_sample), std::end(curr_sample), 0);
			for (int out = 0; out < 2; out++)
			{
				adpcm[out].reset();
			}
			pos = 0;
			frac = 0;
			update = 1;
			playing = true;
			looping = false;
		}

		void keyoff()
		{
			ctrl &= ~0x80;
			playing = false;
		}

		void tick()
		{
			if (((ctrl & 0x40) == 0) && playing)
			{
				if (pos >= loop)
				{
					for (int out = 0; out < 2; out++)
					{
						if (!adpcm[out].looped)
							adpcm[out].save_loop();
					}
				}
				if (ctrl & 8)
				{
					if (pos >= loopend)
					{
						for (int out = 0; out < 2; out++)
						{
							if (!adpcm[out].looped)
								adpcm[out].load_loop();
						}
						pos = loop;
						looping = true;
					}
				}
				else
				{
					if (pos >= end)
					{
						keyoff();
					}
				}
				pos++;
			}
		}
	};

	void get_sample(channel_t *chan)
	{
		for (int i = 0; i < 2; i++)
			chan->prev_sample[i] = chan->curr_sample[i];

		switch (chan->ctrl & 7)
		{
			case 0:
				for (int j = 0; j < 2; j++)
				{
					chan->curr_sample[j] = chan->adpcm[j].update((m_sampledata->read_byte(chan->start + (chan->pos >> 1)) >> ((~chan->pos & 1) << 2)) & 0xf);
				}
				break;
			case 1:
				for (int j = 0; j < 2; j++)
				{
					chan->curr_sample[j] = s16(m_sampledata->read_byte(chan->start + chan->pos) << 8);
				}
				break;
			case 2:
				for (int j = 0; j < 2; j++)
				{
					chan->curr_sample[j] = m_mulaw[m_sampledata->read_byte(chan->start + chan->pos)];
				}
				break;
			case 3:
				for (int j = 0; j < 2; j++)
				{
					chan->curr_sample[j] = s16(m_sampledata->read_byte(chan->start + (chan->pos * 2)) | (m_sampledata->read_byte(chan->start + (chan->pos * 2) + 1) << 8));
				}
				break;
			case 4:
				chan->curr_sample[0] = chan->adpcm[0].update((m_sampledata->read_byte(chan->start + chan->pos) >> 4) & 0xf);
				chan->curr_sample[1] = chan->adpcm[1].update((m_sampledata->read_byte(chan->start + chan->pos) >> 0) & 0xf);
				break;
			case 5:
				chan->curr_sample[0] = s16(m_sampledata->read_byte(chan->start + (chan->pos * 2)) << 8);
				chan->curr_sample[1] = s16(m_sampledata->read_byte(chan->start + (chan->pos * 2) + 1) << 8);
				break;
			case 6:
				chan->curr_sample[0] = m_mulaw[m_sampledata->read_byte(chan->start + (chan->pos * 2))];
				chan->curr_sample[1] = m_mulaw[m_sampledata->read_byte(chan->start + (chan->pos * 2) + 1)];
				break;
			case 7:
				chan->curr_sample[0] = s16(m_sampledata->read_byte(chan->start + (chan->pos * 4) + 0) | (m_sampledata->read_byte(chan->start + (chan->pos * 4) + 1) << 8));
				chan->curr_sample[1] = s16(m_sampledata->read_byte(chan->start + (chan->pos * 4) + 2) | (m_sampledata->read_byte(chan->start + (chan->pos * 4) + 3) << 8));
				break;
		}
	}

	s32 m_mulaw[256];
	channel_t m_channel[64];
	sound_stream *m_stream;
	address_space_config m_sampledata_config;
	address_space *m_sampledata;
	u16 m_channel_offs;
	u32 m_keyon_ex;
};

DECLARE_DEVICE_TYPE(JKMS8P64, jkms8p64_device)

#endif // MAME_SOUND_JKMS8P64_H
