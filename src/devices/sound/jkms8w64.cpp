
// license:BSD-3-Clause
// copyright-holders:MATRIX
/*
	MATRIX JKMS8W64
	64 Channel Wave generator with FM
*/
#include "emu.h"
#include "jkms8w64.h"

DEFINE_DEVICE_TYPE(JKMS8W64, jkms8w64_device, "jkms8w64", "MATRIX JKMS8W64 Wave Generator")

jkms8w64_device::jkms8w64_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, JKMS8W64, tag, owner, clock)
	, device_sound_interface(mconfig, *this)
	, m_waveform(nullptr)
	, m_stream(nullptr)
{
}

void jkms8w64_device::device_start()
{
	m_waveform = make_unique_clear<u16[]>(WAVE_SIZE);
	m_stream = machine().sound().stream_alloc(*this, 0, 2, clock() / 1024);

	for (int ch = 0; ch < MAX_CHANNEL; ch++)
	{
		channel_t *chan = &m_channel[ch];
		save_item(NAME(chan->wave_xor), ch);
		save_item(NAME(chan->reverse), ch);
		save_item(NAME(chan->playing), ch);
		save_item(NAME(chan->fm_ctrl), ch);
		save_item(NAME(chan->ctrl), ch);
		save_item(NAME(chan->freq), ch);
		save_item(NAME(chan->offs), ch);
		save_item(NAME(chan->pos), ch);
		save_item(NAME(chan->shift), ch);
		save_item(NAME(chan->len), ch);
		save_item(NAME(chan->mvol), ch);
		save_item(NAME(chan->tl), ch);
		save_item(NAME(chan->lvol), ch);
		save_item(NAME(chan->rvol), ch);
		save_item(NAME(chan->stack_offs), ch);
		save_item(NAME(chan->stack_lscale), ch);
		save_item(NAME(chan->stack_rscale), ch);
		save_item(NAME(chan->stack_scale), ch);
	}
	save_pointer(NAME(m_waveform), WAVE_SIZE);
	save_item(NAME(m_stack_l));
	save_item(NAME(m_stack_r));
	save_item(NAME(m_wave_offs));
	save_item(NAME(m_chan_offs));
	save_item(NAME(m_stack_offs));
	save_item(NAME(m_keyon_ex));
}

void jkms8w64_device::device_reset()
{
	for (int ch = 0; ch < MAX_CHANNEL; ch++)
	{
		channel_t *chan = &m_channel[ch];
		chan->wave_xor = 0;
		chan->reverse = 0;
		chan->playing = false;
		chan->fm_ctrl = 0;
		chan->ctrl = 0;
		chan->freq = 0;
		chan->offs = 0;
		chan->pos = 0;
		chan->shift = 0;
		chan->len = 0;
		chan->mvol = 0;
		chan->tl = 0;
		chan->lvol = 0;
		chan->rvol = 0;
		std::fill(std::begin(chan->stack_offs), std::end(chan->stack_offs), 0);
		std::fill(std::begin(chan->stack_lscale), std::end(chan->stack_lscale), 0);
		std::fill(std::begin(chan->stack_rscale), std::end(chan->stack_rscale), 0);
		chan->stack_scale = 0;
	}
	std::fill(std::begin(m_stack_l), std::end(m_stack_l), 0);
	std::fill(std::begin(m_stack_r), std::end(m_stack_r), 0);
	m_wave_offs = 0;
	m_chan_offs = 0;
	m_stack_offs = 0;
	m_keyon_ex = 0;
}

void jkms8w64_device::device_clock_changed()
{
	m_stream->set_sample_rate(clock() / 1024);
}

void jkms8w64_device::sound_stream_update(sound_stream &stream, stream_sample_t **inputs, stream_sample_t **outputs, int samples)
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
				if (chan->ctrl & 0x08)
				{
					if ((chan->ctrl & 0x8000) == 0)
						chan->keyon();

					chan->ctrl |= 0x80;
				}
				else if ((chan->ctrl & 0x8000) == 0)
				{
					if (chan->ctrl & 0x80)
						chan->keyoff();

					chan->ctrl &= ~0x80;
				}
			}
			if (m_keyon_ex & 0x40)
			{
				if (chan->ctrl & 0x4000)
					chan->ctrl |= 0x40;
				else if ((chan->ctrl & 0x4000) == 0)
					chan->ctrl &= ~0x40;
			}
			if (((chan->ctrl & 0x40) == 0) && chan->playing)
			{
				s64 stack_l = 0, stack_r = 0;
				if (chan->fm_ctrl & 0x08)
				{
					int stack = 0;
					for (; stack <= (chan->fm_ctrl & 0x03); stack++)
					{
						stack_l += (m_stack_l[(m_stack_offs + chan->stack_offs[stack]) & 0xff] * chan->stack_lscale[stack]) >> 8;
						stack_r += (m_stack_r[(m_stack_offs + chan->stack_offs[stack]) & 0xff] * chan->stack_rscale[stack]) >> 8;
					}
					stack_l = (stack_l * chan->stack_scale) >> 8;
					stack_r = (stack_r * chan->stack_scale) >> 8;
					stack_l /= stack;
					stack_r /= stack;
				}
				u16 wave_pingpong_l = 0, wave_pingpong_r = 0;
				s64 out_l = s16(m_waveform[get_wave_offs(chan, stack_l, wave_pingpong_l)] ^ chan->wave_xor ^ wave_pingpong_l);
				s64 out_r = s16(m_waveform[get_wave_offs(chan, stack_r, wave_pingpong_r)] ^ chan->wave_xor ^ wave_pingpong_r);
				const s32 lvol = (chan->ctrl & 0x100) ? -chan->lvol : chan->lvol;
				const s32 rvol = (chan->ctrl & 0x200) ? -chan->rvol : chan->rvol;
				if (chan->fm_ctrl & 0x20)
				{
					if (chan->fm_ctrl & 0x10)
					{
						m_stack_l[m_stack_offs] = (out_l * chan->tl) >> 8;
						m_stack_r[m_stack_offs] = (out_r * chan->tl) >> 8;
					}
					else
					{
						m_stack_l[m_stack_offs] = (((out_l * chan->mvol) >> 8) * lvol * chan->tl) >> 16;
						m_stack_r[m_stack_offs] = (((out_r * chan->mvol) >> 8) * rvol * chan->tl) >> 16;
					}
				}
				out_l = (out_l * chan->mvol * lvol) >> 16;
				out_r = (out_r * chan->mvol * rvol) >> 16;
				chan->tick();
				outputs[0][sample] += out_l;
				outputs[1][sample] += out_r;
			}
			m_stack_offs++;
		}
		m_keyon_ex &= ~0xc0;
	}
}

u16 jkms8w64_device::word_r(offs_t offset)
{
	u16 ret = 0;
	channel_t *chan = &m_channel[m_chan_offs & 0x3f];
	switch (offset & 15)
	{
		case 0:
			ret = chan->ctrl;
			break;
		case 1:
			ret = chan->freq >> 16;
			break;
		case 2:
			ret = chan->freq & 0xffff;
			break;
		case 3:
			ret = chan->offs;
			break;
		case 4:
			ret = (chan->tl << 8) | chan->shift;
			break;
		case 5:
			ret = (u8(chan->lvol) << 8) | u8(chan->rvol);
			break;
		case 6:
			ret = (chan->stack_offs[0] << 8) | chan->stack_offs[1];
			break;
		case 7:
			ret = (chan->stack_offs[2] << 8) | chan->stack_offs[3];
			break;
		case 8:
			ret = (chan->stack_lscale[0] << 8) | chan->stack_rscale[0];
			break;
		case 9:
			ret = (chan->stack_lscale[1] << 8) | chan->stack_rscale[1];
			break;
		case 10:
			ret = (chan->stack_lscale[2] << 8) | chan->stack_rscale[2];
			break;
		case 11:
			ret = (chan->stack_lscale[3] << 8) | chan->stack_rscale[3];
			break;
		case 12:
			ret = (chan->fm_ctrl << 8) | chan->stack_scale;
			break;
		case 13:
			ret = (chan->mvol << 8) | m_keyon_ex | m_chan_offs;
			break;
		case 14:
			ret = m_wave_offs;
			break;
		case 15:
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
	1	-------- ffffffff Frequency Hi bits (f * (clock / 1024 / 65536))
	2	ffffffff ffffffff Frequency Low bits (f * (clock / 1024 / 65536))
	3	oooooooo oooooooo Wave offset
	4	pppppppp -------- FM Stack output level
		-------- ----llll Wave length (1 << l)
	5	llllllll -------- Left volume
		-------- rrrrrrrr Right volume
	6	iiiiiiii -------- FM Stack input A offset
		-------- iiiiiiii FM Stack input B offset
	7	iiiiiiii -------- FM Stack input C offset
		-------- iiiiiiii FM Stack input D offset
	8	llllllll -------- FM Stack input A Left scale
		-------- rrrrrrrr FM Stack input A Right scale
	9	llllllll -------- FM Stack input B Left scale
		-------- rrrrrrrr FM Stack input B Right scale
	a	llllllll -------- FM Stack input C Left scale
		-------- rrrrrrrr FM Stack input C Right scale
	b	llllllll -------- FM Stack input D Left scale
		-------- rrrrrrrr FM Stack input D Right scale
	c	--o----- -------- FM Stack output enable
		---o---- -------- FM Stack direct output enable
		----i--- -------- FM Stack input enable
		------nn -------- Number of FM Stack inputs
		-------- ssssssss Global FM Stack input scale
	d	mmmmmmmm -------- Master volume
		-------- e------- Execute keyon
		-------- -p------ Execute pause
		-------- --ssssss Channel select
	e	wwwwwwww wwwwwwww Waveram Address
	f	dddddddd dddddddd Waveram data
*/

void jkms8w64_device::word_w(offs_t offset, u16 data, u16 mem_mask)
{
	channel_t *chan = &m_channel[m_chan_offs & 0x3f];
	switch (offset & 15)
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
			if (ACCESSING_BITS_0_7)
			{
				data &= 0xff;
				mem_mask &= 0xff;
				chan->freq = (chan->freq & ~(mem_mask << 16)) | ((data & mem_mask) << 16);
			}
			break;
		case 2:
			chan->freq = (chan->freq & ~mem_mask) | (data & mem_mask);
			break;
		case 3:
			COMBINE_DATA(&chan->offs);
			break;
		case 4:
			if (ACCESSING_BITS_8_15)
				chan->tl = (data >> 8) & 0xff;
			if (ACCESSING_BITS_0_7)
				chan->shift = data & 0xf;
			break;
		case 5:
			if (ACCESSING_BITS_8_15)
				chan->lvol = s8(data >> 8);
			if (ACCESSING_BITS_0_7)
				chan->rvol = s8(data & 0xff);
			break;
		case 6:
			if (ACCESSING_BITS_8_15)
				chan->stack_offs[0] = (data >> 8) & 0xff;
			if (ACCESSING_BITS_0_7)
				chan->stack_offs[1] = data & 0xff;
			break;
		case 7:
			if (ACCESSING_BITS_8_15)
				chan->stack_offs[2] = (data >> 8) & 0xff;
			if (ACCESSING_BITS_0_7)
				chan->stack_offs[3] = data & 0xff;
			break;
		case 8:
			if (ACCESSING_BITS_8_15)
				chan->stack_lscale[0] = (data >> 8) & 0xff;
			if (ACCESSING_BITS_0_7)
				chan->stack_rscale[0] = data & 0xff;
			break;
		case 9:
			if (ACCESSING_BITS_8_15)
				chan->stack_lscale[1] = (data >> 8) & 0xff;
			if (ACCESSING_BITS_0_7)
				chan->stack_rscale[1] = data & 0xff;
			break;
		case 10:
			if (ACCESSING_BITS_8_15)
				chan->stack_lscale[2] = (data >> 8) & 0xff;
			if (ACCESSING_BITS_0_7)
				chan->stack_rscale[2] = data & 0xff;
			break;
		case 11:
			if (ACCESSING_BITS_8_15)
				chan->stack_lscale[3] = (data >> 8) & 0xff;
			if (ACCESSING_BITS_0_7)
				chan->stack_rscale[3] = data & 0xff;
			break;
		case 12:
			if (ACCESSING_BITS_8_15)
				chan->fm_ctrl = (data >> 8) & 0xff;
			if (ACCESSING_BITS_0_7)
				chan->stack_scale = data & 0xff;
			break;
		case 13:
			if (ACCESSING_BITS_8_15)
				chan->mvol = (data >> 8) & 0xff;
			if (ACCESSING_BITS_0_7)
			{
				m_keyon_ex = data & 0xc0;
				m_chan_offs = data & 0x3f;
			}
			break;
		case 14:
			COMBINE_DATA(&m_wave_offs);
			break;
		case 15:
			COMBINE_DATA(&m_waveform[m_wave_offs & WAVE_MASK]);
			break;
	}
}

u8 jkms8w64_device::byte_be_r(offs_t offset)
{
	const u8 shift = (~offset & 1) << 3;
	return (word_r(offset >> 1) >> offset) & 0xff;
}

u8 jkms8w64_device::byte_le_r(offs_t offset)
{
	const u8 shift = (offset & 1) << 3;
	return (word_r(offset >> 1) >> offset) & 0xff;
}

void jkms8w64_device::byte_be_w(offs_t offset, u8 data, u8 mem_mask)
{
	const u8 shift = (~offset & 1) << 3;
	word_w(offset >> 1, data << shift, mem_mask << shift);
}

void jkms8w64_device::byte_le_w(offs_t offset, u8 data, u8 mem_mask)
{
	const u8 shift = (offset & 1) << 3;
	word_w(offset >> 1, data << shift, mem_mask << shift);
}
