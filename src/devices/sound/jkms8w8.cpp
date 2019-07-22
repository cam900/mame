
// license:BSD-3-Clause
// copyright-holders:MATRIX
/*
	MATRIX JKMS8W8
	8 Channel Wave generator
*/
#include "emu.h"
#include "jkms8w8.h"

// jkms8w8.cpp
DEFINE_DEVICE_TYPE(JKMS8W8, jkms8w8_device, "jkms8w8", "MATRIX JKMS8W8 Wave Generator")

jkms8w8_device::jkms8w8_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, JKMS8W8, tag, owner, clock)
	, device_sound_interface(mconfig, *this)
	, m_waveform(nullptr)
	, m_stream(nullptr)
{
}

void jkms8w8_device::device_start()
{
	m_waveform = make_unique_clear<u16[]>(WAVE_SIZE);
	m_stream = machine().sound().stream_alloc(*this, 0, 2, clock() / 16);

	for (int ch = 0; ch < MAX_CHANNEL; ch++)
	{
		channel_t *chan = &m_channel[ch];
		save_item(NAME(chan->wave_clock), ch);
		save_item(NAME(chan->wave_xor), ch);
		save_item(NAME(chan->wave_pingpong), ch);
		save_item(NAME(chan->pingpong), ch);
		save_item(NAME(chan->reverse), ch);
		save_item(NAME(chan->playing), ch);
		save_item(NAME(chan->ctrl), ch);
		save_item(NAME(chan->remain), ch);
		save_item(NAME(chan->freq), ch);
		save_item(NAME(chan->offs), ch);
		save_item(NAME(chan->pos), ch);
		save_item(NAME(chan->len), ch);
		save_item(NAME(chan->mvol), ch);
		save_item(NAME(chan->lvol), ch);
		save_item(NAME(chan->rvol), ch);
	}
	save_pointer(NAME(m_waveform), WAVE_SIZE);
	save_item(NAME(m_wave_offs));
	save_item(NAME(m_chan_offs));
	save_item(NAME(m_keyon_ex));
}

void jkms8w8_device::device_reset()
{
	for (int ch = 0; ch < MAX_CHANNEL; ch++)
	{
		channel_t *chan = &m_channel[ch];
		chan->wave_clock = 0;
		chan->wave_xor = 0;
		chan->wave_pingpong = 0;
		chan->pingpong = 0;
		chan->reverse = 0;
		chan->playing = false;
		chan->ctrl = 0;
		chan->remain = 0;
		chan->freq = 0;
		chan->offs = 0;
		chan->pos = 0;
		chan->len = 0;
		chan->mvol = 0;
		chan->lvol = 0;
		chan->rvol = 0;
	}
	m_wave_offs = 0;
	m_chan_offs = 0;
	m_keyon_ex = 0;
}

void jkms8w8_device::device_clock_changed()
{
	m_stream->set_sample_rate(clock() / 16);
}

void jkms8w8_device::sound_stream_update(sound_stream &stream, stream_sample_t **inputs, stream_sample_t **outputs, int samples)
{
	std::fill_n(&outputs[0][0], samples, 0);
	std::fill_n(&outputs[1][0], samples, 0);
	for (int sample = 0; sample < samples; sample++)
	{
		for (int ch = 0; ch < MAX_CHANNEL; ch++)
		{
			channel_t *chan = &m_channel[ch];
			if (m_keyon_ex & 0x80)
			{
				if ((chan->ctrl & 0x8000) && ((chan->ctrl & 0x80) == 0))
				{
					chan->keyon();
					chan->ctrl |= 0x80;
				}
				else if (((chan->ctrl & 0x8000) == 0) && (chan->ctrl & 0x80))
				{
					chan->keyoff();
					chan->ctrl &= ~0x80;
				}
			}
			if (m_keyon_ex & 0x40)
			{
				if ((chan->ctrl & 0x4000) && ((chan->ctrl & 0x40) == 0))
					chan->ctrl |= 0x40;
				else if (((chan->ctrl & 0x4000) == 0) && (chan->ctrl & 0x40))
					chan->ctrl &= ~0x40;
			}
			if (((chan->ctrl & 0x40) == 0) && chan->playing)
			{
				s64 out_l = 0, out_r = 0;
				s64 out = s16(m_waveform[get_wave_offs(chan)] ^ chan->wave_xor ^ chan->wave_pingpong) * chan->mvol;
				out_l = (out * ((chan->ctrl & 0x100) ? -chan->lvol : chan->lvol)) >> 16;
				out_r = (out * ((chan->ctrl & 0x200) ? -chan->rvol : chan->rvol)) >> 16;
				chan->tick();
				outputs[0][sample] += out_l;
				outputs[1][sample] += out_r;
			}
		}
	}
}

u16 jkms8w8_device::word_r(offs_t offset)
{
	u16 ret = 0;
	channel_t *chan = &m_channel[m_chan_offs & 7];
	switch (offset & 7)
	{
		case 0:
			ret = chan->ctrl;
			break;
		case 1:
			ret = chan->freq;
			break;
		case 2:
			ret = chan->offs;
			break;
		case 3:
			ret = (chan->pos << 8) | chan->len;
			break;
		case 4:
			ret = (chan->lvol << 8) | chan->rvol;
			break;
		case 5:
			ret = (chan->mvol << 8) | m_keyon_ex | m_chan_offs;
			break;
		case 6:
			ret = m_wave_offs;
			break;
		case 7:
			ret = m_waveform[m_wave_offs & WAVE_MASK];
			break;
	}
	return ret;
}

/*
	Register Map

		fedcba98 76543210
	0	e------- -------- Keyon execute bit
		-p------ -------- Pause execute bit
		----w--- -------- Invert waveform
		-----a-- -------- Invert waveaddr
		------r- -------- Invert right output
		-------l -------- Invert left output
		-------- k------- Keyon
		-------- -p------ Pause
		-------- --p----- Waveform pingpong
		-------- ---p---- Waveaddr pingpong
		-------- ----i--- Invert pingpong
		-------- -----mmm Pingpong mode
	1	ffffffff ffffffff Frequency (clock / (f + 1))
	2	----oooo oooooooo Wave offset
	3	pppppppp -------- Wave position
		-------- llllllll Wave length
	4	llllllll -------- Left volume
		-------- rrrrrrrr Right volume
	5	mmmmmmmm -------- Master volume
		-------- e------- Execute keyon
		-------- -p------ Execute pause
		-------- -----sss Channel select
	6	----wwww wwwwwwww Waveram Address
	7	dddddddd dddddddd Waveram data
*/

void jkms8w8_device::word_w(offs_t offset, u16 data, u16 mem_mask)
{
	channel_t *chan = &m_channel[m_chan_offs & 7];
	switch (offset & 7)
	{
		case 0:
			chan->wave_xor = BIT(chan->ctrl, 11) ? 0xffff : 0;
			chan->reverse = BIT(chan->ctrl, 10) ? 1 : 0;
			const u16 old_ctrl = chan->ctrl;
			COMBINE_DATA(&chan->ctrl);
			if (((chan->ctrl & 0x80) == 0x80) && ((old_ctrl & 0x80) == 0))
				chan->keyon();
			else if (((chan->ctrl & 0x80) == 0) && ((old_ctrl & 0x80) == 0x80))
				chan->keyoff();
			break;
		case 1:
			COMBINE_DATA(&chan->freq);
			break;
		case 2:
			COMBINE_DATA(&chan->offs);
			break;
		case 3:
			if (ACCESSING_BITS_8_15)
				chan->pos = (data >> 8) & 0xff;
			if (ACCESSING_BITS_0_7)
				chan->len = data & 0xff;
			break;
		case 4:
			if (ACCESSING_BITS_8_15)
				chan->lvol = (data >> 8) & 0xff;
			if (ACCESSING_BITS_0_7)
				chan->rvol = data & 0xff;
			break;
		case 5:
			if (ACCESSING_BITS_8_15)
				chan->mvol = (data >> 8) & 0xff;
			if (ACCESSING_BITS_0_7)
			{
				m_keyon_ex = data & 0xc0;
				m_chan_offs = data & 7;
			}
			break;
		case 6:
			COMBINE_DATA(&m_wave_offs);
			break;
		case 7:
			COMBINE_DATA(&m_waveform[m_wave_offs & WAVE_MASK]);
			break;
	}
}

u8 jkms8w8_device::byte_be_r(offs_t offset)
{
	const u8 shift = (~offset & 1) << 3;
	return (word_r(offset >> 1) >> offset) & 0xff;
}

u8 jkms8w8_device::byte_le_r(offs_t offset)
{
	const u8 shift = (offset & 1) << 3;
	return (word_r(offset >> 1) >> offset) & 0xff;
}

void jkms8w8_device::byte_be_w(offs_t offset, u8 data, u8 mem_mask)
{
	const u8 shift = (~offset & 1) << 3;
	word_w(offset >> 1, data << shift, mem_mask << shift);
}

void jkms8w8_device::byte_le_w(offs_t offset, u8 data, u8 mem_mask)
{
	const u8 shift = (offset & 1) << 3;
	word_w(offset >> 1, data << shift, mem_mask << shift);
}
