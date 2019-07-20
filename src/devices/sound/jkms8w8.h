
// license:BSD-3-Clause
// copyright-holders:MATRIX
/*
	MATRIX JKMS8W8
	8 Channel Wave generator
*/
#ifndef MAME_SOUND_JKMS8W8_H
#define MAME_SOUND_JKMS8W8_H

#pragma once


class jkms8w8_device : public device_t, public device_sound_interface
{
public:
	jkms8w8_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	u16 word_r(offs_t offset);
	void word_w(offs_t offset, u16 data, u16 mem_mask = ~0);

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
	static constexpr u8 MAX_CHANNEL = 8;
	static constexpr u16 WAVE_SHIFT = 12;
	static constexpr u16 WAVE_SIZE = 1 << WAVE_SHIFT;
	static constexpr u16 WAVE_MASK = WAVE_SIZE - 1;

	struct channel_t
	{
		bool playing = false;
		u32 rng = 0;
		u8 wave_clock = 0;
		u8 wave_xor = 0;
		u8 wave_pingpong = 0;
		u8 pingpong = 0;
		u8 reverse = 0;
		u16 ctrl = 0;
		u16 remain = 0;
		u16 freq = 0;
		u16 offs = 0;
		u8 pos = 0;
		u8 len = 0;
		u8 mvol = 0;
		u8 lvol = 0;
		u8 rvol = 0;

		void keyon()
		{
			remain = freq;
			playing = true;
			pingpong = 0;
			wave_pingpong = (((ctrl & 7) == 1) && (rng & 1)) ? 0xff : 0;
			wave_clock = 0;
		}

		void keyoff()
		{
			playing = false;
		}

		void tick()
		{
			if (((ctrl & 0x40) == 0) && playing)
			{
				if (remain)
				{
					remain--;
				}
				else
				{
					remain = freq;
					pos++;
					if (pos > len)
					{
						pos = 0;
						if (ctrl & 0x20)
						{
							if (ctrl & 0x10)
							{
								switch (ctrl & 7)
								{
									case 0:
										wave_pingpong ^= 0xff;
										pingpong ^= 1;
										break;
									case 1:
										rng ^= (((rng & 1) ^ ((rng >> 3) & 1)) << 17);
										rng >>= 1;
										wave_pingpong = (rng & 1) ? 0xff : 0;
										pingpong = (rng & 8) ? 1 : 0;
										break;
									case 2:
										if (BIT(wave_clock, 0))
											wave_pingpong ^= 0xff;
										pingpong ^= 1;
										break;
									case 3:
										wave_pingpong ^= 0xff;
										if (BIT(wave_clock, 0))
											pingpong ^= 1;
										break;
									case 4:
										if (BIT(wave_pingpong, 0) == BIT(pingpong, 0))
											wave_pingpong ^= 0xff;
										else
											pingpong ^= 1;
										break;
									case 5:
										if (BIT(wave_pingpong, 0) == BIT(pingpong, 0))
											pingpong ^= 1;
										else
											wave_pingpong ^= 0xff;
										break;
									case 6:
										if ((BIT(wave_clock, 0)) ^ (BIT(wave_clock, 1)))
											wave_pingpong ^= 0xff;
										else
											pingpong ^= 1;
										break;
									case 7:
										if ((BIT(wave_clock, 0)) ^ (BIT(wave_clock, 1)))
											pingpong ^= 1;
										else
											wave_pingpong ^= 0xff;
										break;
								}
								wave_clock++;
							}
							else
								wave_pingpong ^= 0xff;
						}
						else
						{
							if (ctrl & 0x10)
								pingpong ^= 1;
						}
						
					}
				}
			}
		}
	};

	u16 get_wave_offs(channel_t *chan) { return (chan->offs + ((chan->reverse ^ chan->pingpong) ? (chan->len - chan->pos - 1) : chan->pos)) & WAVE_MASK; }

	channel_t m_channel[8];
	std::unique_ptr<u8[]> m_waveform;
	sound_stream *m_stream;
	u16 m_wave_offs;
	u8 m_chan_offs;
	u8 m_keyon_ex;
};

DECLARE_DEVICE_TYPE(JKMS8W8, jkms8w8_device)

#endif // MAME_SOUND_JKMS8W8_H
