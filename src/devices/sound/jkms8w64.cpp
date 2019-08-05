
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
		save_item(NAME(chan->len), ch);
		save_item(NAME(chan->mvol), ch);
		save_item(NAME(chan->tl), ch);
		save_item(NAME(chan->lvol), ch);
		save_item(NAME(chan->rvol), ch);
		save_item(NAME(chan->output), ch);
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
	save_item(NAME(m_total_output));
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
		std::fill(std::begin(m_total_output), std::end(m_total_output), 0);
		for (int ch = 0; ch < MAX_CHANNEL; ch++)
		{
			channel_t *chan = &m_channel[ch];
			std::fill(std::begin(chan->output), std::end(chan->output), 0);
			if (m_keyon_ex & 0x2)
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
			if (m_keyon_ex & 0x1)
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
					stack_l = (stack_l * chan->stack_scale) >> 16;
					stack_r = (stack_r * chan->stack_scale) >> 16;
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
						m_stack_l[m_stack_offs] = (out_l * chan->tl) >> 16;
						m_stack_r[m_stack_offs] = (out_r * chan->tl) >> 16;
					}
					else
					{
						m_stack_l[m_stack_offs] = (((out_l * chan->mvol) >> 16) * lvol * chan->tl) >> 32;
						m_stack_r[m_stack_offs] = (((out_r * chan->mvol) >> 16) * rvol * chan->tl) >> 32;
					}
				}
				out_l *= chan->mvol * lvol;
				out_r *= chan->mvol * rvol;
				chan->output[0] = out_l;
				chan->output[1] = out_r;
				chan->tick();
				for (int out = 0; out < 2; out++)
					m_total_output[out] += chan->output[out];
			}
			m_stack_offs++;
		}
		outputs[0][sample] += m_total_output[0] >> 38;
		outputs[1][sample] += m_total_output[1] >> 38;
		m_keyon_ex = 0;
	}
}

u64 jkms8w64_device::qword_r(offs_t offset)
{
	u16 ret = 0;
	channel_t *chan = &m_channel[m_chan_offs & 0x3f];
	switch (offset & 15)
	{
		case 0:
			ret = (chan->freq << 32) | (chan->tl << 16) | chan->ctrl;
			break;
		case 1:
			ret = (chan->offs << 48) | (chan->len << 32) | (chan->lvol << 16) | chan->rvol;
			break;
		case 2:
			ret = (chan->stack_lscale[0] << 56) | (chan->stack_rscale[0] << 48) | 
					(chan->stack_lscale[1] << 40) | (chan->stack_rscale[1] << 32) | 
					(chan->stack_lscale[2] << 24) | (chan->stack_rscale[2] << 16) | 
					(chan->stack_lscale[3] << 8) | chan->stack_rscale[3];
			break;
		case 3:
			ret = (chan->stack_offs[0] << 56) | (chan->stack_offs[1] << 48) | (chan->stack_offs[2] << 40) | (chan->stack_offs[3] << 32) |
					(chan->stack_scale << 16) | chan->fm_ctrl;
			break;
		case 4:
		case 5:
		case 6:
		case 7:
			ret = (m_wave_offs << 48) | (m_waveform[m_wave_offs & WAVE_MASK] << 32) | (chan->mvol << 16) | (m_keyon_ex << 8) | m_chan_offs;
			break;
		case 8:
		case 12:
			return chan->output[0];
		case 9:
		case 13:
			return chan->output[1];
		case 10:
		case 14:
			return m_total_output[0];
		case 11:
		case 15:
			return m_total_output[1];
	}
	return ret;
}

/*
	Register Map (64bit)

		fedcba98 76543210 fedcba98 76543210 fedcba98 76543210 fedcba98 76543210
	0	-------- ffffffff ffffffff ffffffff -------- -------- -------- -------- Frequency (f * (clock / 1024 / 65536))
		-------- -------- -------- -------- pppppppp pppppppp -------- -------- FM Stack output level
		-------- -------- -------- -------- -------- -------- e------- -------- Keyon execute bit
		-------- -------- -------- -------- -------- -------- -p------ -------- Pause execute bit
		-------- -------- -------- -------- -------- -------- ----w--- -------- Invert waveform
		-------- -------- -------- -------- -------- -------- -----a-- -------- Invert waveaddr
		-------- -------- -------- -------- -------- -------- ------r- -------- Invert right output
		-------- -------- -------- -------- -------- -------- -------l -------- Invert left output
		-------- -------- -------- -------- -------- -------- -------- k------- Keyon
		-------- -------- -------- -------- -------- -------- -------- -p------ Pause
		-------- -------- -------- -------- -------- -------- -------- --p----- Waveform pingpong
		-------- -------- -------- -------- -------- -------- -------- ---p---- Waveaddr pingpong
		-------- -------- -------- -------- -------- -------- -------- ----i--- Invert pingpong
		-------- -------- -------- -------- -------- -------- -------- -----mmm Pingpong mode
	1	oooooooo oooooooo -------- -------- -------- -------- -------- -------- Wave offset
		-------- -------- llllllll llllllll -------- -------- -------- -------- Wave length + 1
		-------- -------- -------- -------- llllllll llllllll -------- -------- Left volume
		-------- -------- -------- -------- -------- -------- rrrrrrrr rrrrrrrr Right volume
	2	llllllll -------- -------- -------- -------- -------- -------- -------- FM Stack input A Left scale
		-------- rrrrrrrr -------- -------- -------- -------- -------- -------- FM Stack input A Right scale
		-------- -------- llllllll -------- -------- -------- -------- -------- FM Stack input B Left scale
		-------- -------- -------- rrrrrrrr -------- -------- -------- -------- FM Stack input B Right scale
		-------- -------- -------- -------- llllllll -------- -------- -------- FM Stack input C Left scale
		-------- -------- -------- -------- -------- rrrrrrrr -------- -------- FM Stack input C Right scale
		-------- -------- -------- -------- -------- -------- llllllll -------- FM Stack input D Left scale
		-------- -------- -------- -------- -------- -------- -------- rrrrrrrr FM Stack input D Right scale
	3	iiiiiiii -------- -------- -------- -------- -------- -------- -------- FM Stack input A offset
		-------- iiiiiiii -------- -------- -------- -------- -------- -------- FM Stack input B offset
		-------- -------- iiiiiiii -------- -------- -------- -------- -------- FM Stack input C offset
		-------- -------- -------- iiiiiiii -------- -------- -------- -------- FM Stack input D offset
		-------- -------- -------- -------- ssssssss ssssssss -------- -------- Global FM Stack input scale
		-------- -------- -------- -------- -------- -------- -------- --o----- FM Stack output enable
		-------- -------- -------- -------- -------- -------- -------- ---o---- FM Stack direct output enable
		-------- -------- -------- -------- -------- -------- -------- ----i--- FM Stack input enable
		-------- -------- -------- -------- -------- -------- -------- ------nn Number of FM Stack inputs
	4*	wwwwwwww wwwwwwww -------- -------- -------- -------- -------- -------- Waveram Address
		-------- -------- dddddddd dddddddd -------- -------- -------- -------- Waveram data
		-------- -------- -------- -------- mmmmmmmm mmmmmmmm -------- -------- Master volume
		-------- -------- -------- -------- -------- -------- ------e- -------- Execute keyon
		-------- -------- -------- -------- -------- -------- -------p -------- Execute pause
		-------- -------- -------- -------- -------- -------- -------- --ssssss Channel select
		* : 01xx (x : don't care)
*/

void jkms8w64_device::qword_w(offs_t offset, u64 data, u64 mem_mask)
{
	channel_t *chan = &m_channel[m_chan_offs & 0x3f];
	switch (offset & 15)
	{
		case 0:
			if (ACCESSING_BITS_0_15)
			{
				const u16 old_ctrl = chan->ctrl;
				chan->ctrl = ((chan->ctrl & ~mem_mask) | (data & mem_mask)) & 0xffff;
				chan->wave_xor = BIT(chan->ctrl, 11) ? 0xffff : 0;
				chan->reverse = BIT(chan->ctrl, 10) ? 1 : 0;
				if (((chan->ctrl & 0x80) == 0x80) && ((old_ctrl & 0x80) == 0))
					chan->keyon();
				else if (((chan->ctrl & 0x80) == 0) && ((old_ctrl & 0x80) == 0x80))
					chan->keyoff();
			}
			if (ACCESSING_BITS_16_31)
			{
				chan->tl = ((chan->tl & ~(mem_mask >> 16)) | ((data & mem_mask) >> 16)) & 0xffff;
			}
			if (ACCESSING_BITS_32_63)
			{
				data = (data >> 32) & 0xffffff;
				mem_mask = (mem_mask >> 32) & 0xffffff;
				COMBINE_DATA(&chan->freq);
			}
			break;
		case 1:
			if (ACCESSING_BITS_48_63)
			{
				chan->offs = ((chan->offs & ~(mem_mask >> 48)) | ((data & mem_mask) >> 48)) & 0xffff;
			}
			if (ACCESSING_BITS_32_47)
			{
				chan->len = ((chan->len & ~(mem_mask >> 32)) | ((data & mem_mask) >> 32)) & 0xffff;
			}
			if (ACCESSING_BITS_16_31)
			{
				chan->lvol = ((chan->lvol & ~(mem_mask >> 16)) | ((data & mem_mask) >> 16)) & 0xffff;
			}
			if (ACCESSING_BITS_0_15)
			{
				chan->rvol = ((chan->rvol & ~mem_mask) | (data & mem_mask)) & 0xffff;
			}
			break;
		case 2:
			if (ACCESSING_BITS_56_63)
				chan->stack_lscale[0] = (data >> 56) & 0xff;
			if (ACCESSING_BITS_48_55)
				chan->stack_rscale[0] = (data >> 48) & 0xff;
			if (ACCESSING_BITS_40_47)
				chan->stack_lscale[1] = (data >> 40) & 0xff;
			if (ACCESSING_BITS_32_39)
				chan->stack_rscale[1] = (data >> 32) & 0xff;
			if (ACCESSING_BITS_24_31)
				chan->stack_lscale[2] = (data >> 24) & 0xff;
			if (ACCESSING_BITS_16_23)
				chan->stack_rscale[2] = (data >> 16) & 0xff;
			if (ACCESSING_BITS_8_15)
				chan->stack_lscale[3] = (data >> 8) & 0xff;
			if (ACCESSING_BITS_0_7)
				chan->stack_rscale[3] = data & 0xff;
			break;
		case 3:
			if (ACCESSING_BITS_56_63)
				chan->stack_offs[0] = (data >> 56) & 0xff;
			if (ACCESSING_BITS_48_55)
				chan->stack_offs[1] = (data >> 48) & 0xff;
			if (ACCESSING_BITS_40_47)
				chan->stack_offs[2] = (data >> 40) & 0xff;
			if (ACCESSING_BITS_32_39)
				chan->stack_offs[3] = (data >> 32) & 0xff;
			if (ACCESSING_BITS_16_31)
				chan->stack_scale = ((chan->stack_scale & ~(mem_mask >> 16)) | ((data & mem_mask) >> 16)) & 0xffff;
			if (ACCESSING_BITS_0_15)
				chan->fm_ctrl = ((chan->fm_ctrl & ~mem_mask) | (data & mem_mask)) & 0xffff;
			break;
		case 4:
		case 5:
		case 6:
		case 7:
			if (ACCESSING_BITS_48_63)
			{
				m_wave_offs = ((m_wave_offs & ~(mem_mask >> 48)) | ((data & mem_mask) >> 48)) & 0xffff;
			}
			if (ACCESSING_BITS_32_47)
			{
				m_waveform[m_wave_offs & WAVE_MASK] = ((m_waveform[m_wave_offs & WAVE_MASK] & ~(mem_mask >> 32)) | ((data & mem_mask) >> 32)) & 0xffff;
			}
			if (ACCESSING_BITS_16_31)
			{
				chan->mvol = ((chan->mvol & ~(mem_mask >> 16)) | ((data & mem_mask) >> 16)) & 0xffff;
			}
			if (ACCESSING_BITS_8_15)
			{
				m_keyon_ex = data & 0x3;
			}
			if (ACCESSING_BITS_0_7)
			{
				m_chan_offs = data & 0x3f;
			}
			break;
	}
}

u32 jkms8w64_device::dword_be_r(offs_t offset)
{
	const u8 shift = (~offset & 1) << 5;
	return (qword_r(offset >> 1) >> shift) & 0xffffffff;
}

u32 jkms8w64_device::dword_le_r(offs_t offset)
{
	const u8 shift = (offset & 1) << 5;
	return (qword_r(offset >> 1) >> shift) & 0xffffffff;
}

void jkms8w64_device::dword_be_w(offs_t offset, u32 data, u32 mem_mask)
{
	const u8 shift = (~offset & 1) << 5;
	qword_w(offset >> 1, data << shift, mem_mask << shift);
}

void jkms8w64_device::dword_le_w(offs_t offset, u32 data, u32 mem_mask)
{
	const u8 shift = (offset & 1) << 5;
	qword_w(offset >> 1, data << shift, mem_mask << shift);
}

u16 jkms8w64_device::word_be_r(offs_t offset)
{
	const u8 shift = (~offset & 3) << 4;
	return (qword_r(offset >> 2) >> shift) & 0xffff;
}

u16 jkms8w64_device::word_le_r(offs_t offset)
{
	const u8 shift = (offset & 3) << 4;
	return (qword_r(offset >> 2) >> shift) & 0xffff;
}

void jkms8w64_device::word_be_w(offs_t offset, u16 data, u16 mem_mask)
{
	const u8 shift = (~offset & 3) << 4;
	qword_w(offset >> 2, data << shift, mem_mask << shift);
}

void jkms8w64_device::word_le_w(offs_t offset, u16 data, u16 mem_mask)
{
	const u8 shift = (offset & 3) << 4;
	qword_w(offset >> 2, data << shift, mem_mask << shift);
}

u8 jkms8w64_device::byte_be_r(offs_t offset)
{
	const u8 shift = (~offset & 7) << 3;
	return (qword_r(offset >> 3) >> shift) & 0xff;
}

u8 jkms8w64_device::byte_le_r(offs_t offset)
{
	const u8 shift = (offset & 7) << 3;
	return (qword_r(offset >> 3) >> shift) & 0xff;
}

void jkms8w64_device::byte_be_w(offs_t offset, u8 data, u8 mem_mask)
{
	const u8 shift = (~offset & 7) << 3;
	qword_w(offset >> 3, data << shift, mem_mask << shift);
}

void jkms8w64_device::byte_le_w(offs_t offset, u8 data, u8 mem_mask)
{
	const u8 shift = (offset & 7) << 3;
	qword_w(offset >> 3, data << shift, mem_mask << shift);
}
