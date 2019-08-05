
// license:BSD-3-Clause
// copyright-holders:MATRIX
/*
	MATRIX JKMS8W128E
	128 Channel Wave generator with FM
	96 bit Internal DAC
	Envelope generator integrated
*/
#ifndef MAME_SOUND_JKMS8W128E_H
#define MAME_SOUND_JKMS8W128E_H

#pragma once


class jkms8w128e_device : public device_t, public device_sound_interface
{
public:
	static constexpr u16 MAX_CHANNEL = 128;
	static constexpr u16 OVERALL_OUTPUT_L = MAX_CHANNEL * 2;
	static constexpr u16 OVERALL_OUTPUT_R = (MAX_CHANNEL * 2) + 1;

	jkms8w128e_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

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
	static constexpr u16 WAVE_SHIFT = 16;
	static constexpr u16 WAVE_SIZE = 1 << WAVE_SHIFT;
	static constexpr u16 WAVE_MASK = WAVE_SIZE - 1;

	struct eg_t
	{
		enum {
			EG_DELAY = 0,
			EG_ATTACK,
			EG_HOLD,
			EG_DECAY,
			EG_SUSTAIN,
			EG_RELEASE,
			EG_WAIT,
			EG_KEYOFF
		};

		bool enable = false;
		u32 delay_time = 0;
		u32 attack_time = 0;
		u32 hold_time = 0;
		u32 decay_time = 0;
		u32 sustain_time = 0;
		s32 delay_step = 0;
		s32 attack_step = 0;
		s32 hold_step = 0;
		s32 decay_step = 0;
		s32 sustain_step = 0;
		u32 release_step = 0;
		s32 delay_add = 0;
		s32 attack_add = 0;
		s32 hold_add = 0;
		s32 decay_add = 0;
		s32 sustain_add = 0;
		u32 init_level = 0;
		s64 curr_time = 0;
		s64 curr_step = 0;
		s64 level = 0;
		u8 curr_state = 0;

		void keyon()
		{
			if (enable)
			{
				curr_state = EG_DELAY;
				curr_time = delay_time;
				curr_step = delay_add;
				level = init_level;
			}
			else
			{
				curr_state = EG_WAIT;
				level = 1 << 32;
			}
		}

		void end()
		{
			curr_state = EG_KEYOFF;
			level = 0;
		}

		void keyoff()
		{
			if ((enable) && (release_step > 0))
			{
				curr_state = EG_RELEASE;
				curr_step = release_step;
			}
			else
			{
				end();
			}
		}

		void tick()
		{
			if (enable)
			{
				if (curr_state == EG_DELAY)
				{
					if (curr_time)
					{
						level += curr_step;
						curr_step += delay_add;
						curr_time--;
					}
					else
					{
						curr_state = EG_ATTACK;
					}
				}
				if (curr_state == EG_ATTACK)
				{
					if (curr_time)
					{
						level += curr_step;
						curr_step += attack_add;
						curr_time--;
					}
					else
					{
						curr_state = EG_HOLD;
					}
				}
				if (curr_state == EG_HOLD)
				{
					if (curr_time)
					{
						level += curr_step;
						curr_step += hold_add;
						curr_time--;
					}
					else
					{
						curr_state = EG_DECAY;
					}
				}
				if (curr_state == EG_DECAY)
				{
					if (curr_time)
					{
						level += curr_step;
						curr_step += decay_add;
						curr_time--;
					}
					else
					{
						curr_state = EG_SUSTAIN;
					}
				}
				if (curr_state == EG_SUSTAIN)
				{
					if (curr_time)
					{
						level += curr_step;
						curr_step += sustain_add;
						curr_time--;
					}
					else
					{
						curr_state = EG_WAIT;
					}
				}
				if (curr_state == EG_RELEASE)
				{
					if (level > 0)
					{
						level = std::max(s64(0), level - curr_step);
					}
					else
					{
						end();
					}
				}
				level = std::clamp(level, s64(0), s64(1 << 32));
			}
			else
			{
				level = 1 << 32;
			}
		}
	};

	struct weg_t
	{
		bool enable = false;
		bool playing = false;
		u8 hold_timing = 0;
		u16 wave_xor = 0;
		u16 reverse = 0;
		u32 freq = 0;
		u32 frac = 0;
		u16 offs = 0;
		u64 pos = 0;
		u16 len = 0;
		u16 ctrl = 0;
		u16 wave_clock = 0;

		void keyon()
		{
			frac = freq;
			pos = 0;
			wave_clock = 0;
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
				if ((wave_clock < hold_timing) || (ctrl & 0x1000))
				{
					if (!frac)
					{
						frac = freq;
						pos++;
						if (pos > len)
						{
							pos = 0;
							wave_clock++;
						}
					}
					else
					{
						frac--;
					}
				}
			}
		}
	};

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
		eg_t eg;
		weg_t weg;

		void keyon()
		{
			pos = 0;
			playing = true;
			eg.keyon();
			weg.keyon();
		}

		void keyoff()
		{
			playing = false;
			weg.keyoff();
		}

		void tick()
		{
			if (eg.curr_state == eg.EG_KEYOFF)
			{
				keyoff();
			}
			if (((ctrl & 0x40) == 0) && playing)
			{
				pos += freq;
			}
			eg.tick();
			weg.tick();
		}
	};

	u16 get_wave_offs(channel_t *chan, s32 stack, u16 &wave_pingpong)
	{
		const u32 len = chan->len + 1;
		u64 pos = ((chan->pos >> 20) + stack) % len;
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

	u16 get_weg_wave_offs(channel_t *chan, u16 &wave_pingpong)
	{
		u64 pos = chan->weg.pos;
		u8 wave_clock = chan->weg.wave_clock & 3;
		u8 pingpong = 0;
		if (chan->weg.ctrl & 0x20)
		{
			if (chan->weg.ctrl & 0x10)
			{
				if (chan->weg.ctrl & 8)
					wave_clock ^= 3;

				switch (chan->weg.ctrl & 7)
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
		else if (chan->weg.ctrl & 0x10)
		{
			pingpong = BIT(wave_clock, 0) ? 1 : 0;
		}
		if (pingpong ^ chan->weg.reverse) pos = chan->weg.len - pos;
		return (chan->weg.offs + pos) & WAVE_MASK;
	}

	channel_t m_channel[128];
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

DECLARE_DEVICE_TYPE(JKMS8W128E, jkms8w128e_device)

#endif // MAME_SOUND_JKMS8W128E_H
