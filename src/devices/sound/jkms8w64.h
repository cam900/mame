
// license:BSD-3-Clause
// copyright-holders:MATRIX
/*
	MATRIX JKMS8W64
	64 Channel Wave generator with FM
*/
#ifndef MAME_SOUND_JKMS8W64_H
#define MAME_SOUND_JKMS8W64_H

#pragma once


class jkms8w64_device : public device_t, public device_sound_interface
{
public:
	jkms8w64_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	u64 qword_r(offs_t offset);
	void qword_w(offs_t offset, u64 data, u64 mem_mask = ~0);

	u32 dword_be_r(offs_t offset);
	void dword_be_w(offs_t offset, u32 data, u32 mem_mask = ~0);

	u32 dword_le_r(offs_t offset);
	void dword_le_w(offs_t offset, u32 data, u32 mem_mask = ~0);

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

private:
	static constexpr u8 MAX_CHANNEL = 64;
	static constexpr u16 WAVE_SHIFT = 16;
	static constexpr u16 WAVE_SIZE = 1 << WAVE_SHIFT;
	static constexpr u16 WAVE_MASK = WAVE_SIZE - 1;

	struct channel_t
	{
		bool playing = false;
		u16 wave_xor = 0;
		u16 reverse = 0;
		u16 fm_ctrl = 0;
		u16 ctrl = 0;
		u32 freq = 0;
		u16 offs = 0;
		u64 pos = 0;
		u32 len = 0;
		u16 mvol = 0;
		u16 tl = 0;
		u16 lvol = 0;
		u16 rvol = 0;
		s64 output[2] = {0, 0};
		u8 stack_offs[4] = {0,0,0,0};
		u8 stack_lscale[4] = {0,0,0,0};
		u8 stack_rscale[4] = {0,0,0,0};
		u16 stack_scale = 0;

		void keyon()
		{
			pos = 0;
			playing = true;
		}

		void keyoff()
		{
			playing = false;
		}

		void tick()
		{
			if (((ctrl & 0x40) == 0) && playing)
			{
				pos += freq;
			}
		}
	};

	u16 get_wave_offs(channel_t *chan, s32 stack, u16 &wave_pingpong)
	{
		const u32 len = chan->len + 1;
		u64 pos = ((chan->pos >> 16) + stack) % len;
		u8 wave_clock = (pos / len) & 3;
		u8 pingpong = 0;
		if (chan->ctrl & 0x20)
		{
			if (chan->ctrl & 0x10)
			{
				if (chan->ctrl & 8)
					wave_clock ^= 3;

				switch (chan->ctrl & 7)
				{
					case 0:
					case 1:
						wave_pingpong = BIT(wave_clock, 0) ? 0xffff : 0;
						pingpong = BIT(wave_clock, 0) ? 1 : 0;
						break;
					case 2:
						wave_pingpong = BIT(wave_clock, 1) ? 0xffff : 0;
						pingpong = BIT(wave_clock, 0) ? 1 : 0;
						break;
					case 3:
						wave_pingpong = BIT(wave_clock, 0) ? 0xffff : 0;
						pingpong = BIT(wave_clock, 1) ? 1 : 0;
						break;
					case 4:
						wave_pingpong = BIT(wave_clock, 0) ^ BIT(wave_clock, 1) ? 0xffff : 0;
						pingpong = BIT(wave_clock, 0) ? 1 : 0;
						break;
					case 5:
						wave_pingpong = BIT(wave_clock, 0) ? 0xffff : 0;
						pingpong = BIT(wave_clock, 0) ^ BIT(wave_clock, 1) ? 1 : 0;
						break;
					case 6:
						wave_pingpong = BIT(wave_clock, 0) ^ BIT(wave_clock, 1) ? 0xffff : 0;
						pingpong = BIT(wave_clock, 1) ? 1 : 0;
						break;
					case 7:
						wave_pingpong = BIT(wave_clock, 1) ? 0xffff : 0;
						pingpong = BIT(wave_clock, 0) ^ BIT(wave_clock, 1) ? 1 : 0;
						break;
				}
			}
			else
				wave_pingpong = BIT(wave_clock, 0) ? 0xffff : 0;
		}
		else if (chan->ctrl & 0x10)
		{
			pingpong = BIT(wave_clock, 0) ? 1 : 0;
		}
		if (pingpong ^ chan->reverse) pos = chan->len - pos;
		return (chan->offs + pos) & WAVE_MASK;
	}

	channel_t m_channel[64];
	std::unique_ptr<u16[]> m_waveform;
	s64 m_stack_l[MAX_CHANNEL * 4];
	s64 m_stack_r[MAX_CHANNEL * 4];
	sound_stream *m_stream;
	u16 m_wave_offs;
	u8 m_chan_offs;
	u8 m_stack_offs;
	u8 m_keyon_ex;
	s64 m_total_output[2];
};

DECLARE_DEVICE_TYPE(JKMS8W64, jkms8w64_device)

#endif // MAME_SOUND_JKMS8W64_H
