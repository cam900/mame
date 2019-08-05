
// license:BSD-3-Clause
// copyright-holders:MATRIX
/*
	MATRIX JKMS8W128E
	128 Channel Wave generator with FM
	128 bit Internal DAC
	Envelope generator integrated
*/
#include "emu.h"
#include "jkms8w128e.h"

DEFINE_DEVICE_TYPE(JKMS8W128E, jkms8w128e_device, "jkms8w128e", "MATRIX JKMS8W128E Wave Generator")

jkms8w128e_device::jkms8w128e_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, JKMS8W128E, tag, owner, clock)
	, device_sound_interface(mconfig, *this)
	, m_waveform(nullptr)
	, m_stream(nullptr)
{
}

void jkms8w128e_device::device_start()
{
	m_waveform = make_unique_clear<u16[]>(WAVE_SIZE);
	m_stream = machine().sound().stream_alloc(*this, 0, (MAX_CHANNEL * 2) + 2, clock() / 128);

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
		save_item(NAME(chan->eg.enable), ch);
		save_item(NAME(chan->eg.delay_time), ch);
		save_item(NAME(chan->eg.attack_time), ch);
		save_item(NAME(chan->eg.hold_time), ch);
		save_item(NAME(chan->eg.decay_time), ch);
		save_item(NAME(chan->eg.sustain_time), ch);
		save_item(NAME(chan->eg.delay_step), ch);
		save_item(NAME(chan->eg.attack_step), ch);
		save_item(NAME(chan->eg.hold_step), ch);
		save_item(NAME(chan->eg.decay_step), ch);
		save_item(NAME(chan->eg.sustain_step), ch);
		save_item(NAME(chan->eg.release_step), ch);
		save_item(NAME(chan->eg.delay_add), ch);
		save_item(NAME(chan->eg.attack_add), ch);
		save_item(NAME(chan->eg.hold_add), ch);
		save_item(NAME(chan->eg.decay_add), ch);
		save_item(NAME(chan->eg.sustain_add), ch);
		save_item(NAME(chan->eg.init_level), ch);
		save_item(NAME(chan->eg.curr_time), ch);
		save_item(NAME(chan->eg.curr_step), ch);
		save_item(NAME(chan->eg.level), ch);
		save_item(NAME(chan->eg.curr_state), ch);
		save_item(NAME(chan->weg.enable), ch);
		save_item(NAME(chan->weg.playing), ch);
		save_item(NAME(chan->weg.hold_timing), ch);
		save_item(NAME(chan->weg.wave_xor), ch);
		save_item(NAME(chan->weg.reverse), ch);
		save_item(NAME(chan->weg.freq), ch);
		save_item(NAME(chan->weg.frac), ch);
		save_item(NAME(chan->weg.offs), ch);
		save_item(NAME(chan->weg.pos), ch);
		save_item(NAME(chan->weg.len), ch);
		save_item(NAME(chan->weg.ctrl), ch);
		save_item(NAME(chan->weg.wave_clock), ch);
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

void jkms8w128e_device::device_reset()
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
		chan->eg.enable = false;
		chan->eg.delay_time = 0;
		chan->eg.attack_time = 0;
		chan->eg.hold_time = 0;
		chan->eg.decay_time = 0;
		chan->eg.sustain_time = 0;
		chan->eg.attack_step = 0;
		chan->eg.hold_step = 0;
		chan->eg.decay_step = 0;
		chan->eg.sustain_step = 0;
		chan->eg.release_step = 0;
		chan->eg.delay_add = 0;
		chan->eg.attack_add = 0;
		chan->eg.hold_add = 0;
		chan->eg.decay_add = 0;
		chan->eg.sustain_add = 0;
		chan->eg.init_level = 0;
		chan->eg.curr_time = 0;
		chan->eg.curr_step = 0;
		chan->eg.level = 0;
		chan->eg.curr_state = chan->eg.EG_KEYOFF;
		chan->weg.enable = false;
		chan->weg.playing = false;
		chan->weg.hold_timing = 0;
		chan->weg.wave_xor = 0;
		chan->weg.reverse = 0;
		chan->weg.freq = 0;
		chan->weg.frac = 0;
		chan->weg.offs = 0;
		chan->weg.pos = 0;
		chan->weg.len = 0;
		chan->weg.ctrl = 0;
		chan->weg.wave_clock = 0;
	}
	std::fill(std::begin(m_stack_l), std::end(m_stack_l), 0);
	std::fill(std::begin(m_stack_r), std::end(m_stack_r), 0);
	m_wave_offs = 0;
	m_chan_offs = 0;
	m_stack_offs = 0;
	m_keyon_ex = 0;
}

void jkms8w128e_device::device_clock_changed()
{
	m_stream->set_sample_rate(clock() / 128);
}

void jkms8w128e_device::sound_stream_update(sound_stream &stream, stream_sample_t **inputs, stream_sample_t **outputs, int samples)
{
	for (int ch = 0; ch <= MAX_CHANNEL; ch++)
	{
		for (int out = 0; out < 2; out++)
			std::fill_n(&outputs[(ch * 2) + out][0], samples, 0);
	}
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
				if (chan->eg.enable)
				{
					out_l = (out_l * chan->eg.level) >> 32;
					out_r = (out_r * chan->eg.level) >> 32;
				}
				if (chan->weg.enable)
				{
					u16 weg_pingpong = 0;
					s64 weg_out;
					if (chan->weg.ctrl & 0x2000)
					{
						weg_out = m_waveform[get_weg_wave_offs(chan, weg_pingpong)] ^ chan->weg.wave_xor ^ weg_pingpong;
						out_l = (out_l * weg_out) >> 16;
						out_r = (out_r * weg_out) >> 16;
					}
					else
					{
						weg_out = s16(m_waveform[get_weg_wave_offs(chan, weg_pingpong)] ^ chan->weg.wave_xor ^ weg_pingpong);
						out_l = (out_l * weg_out) >> 15;
						out_r = (out_r * weg_out) >> 15;
					}
				}
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
			for (int out = 0; out < 2; out++)
				outputs[(ch * 2) + out][sample] += chan->output[out] >> 32;

			m_stack_offs++;
		}
		for (int out = 0; out < 2; out++)
			outputs[(MAX_CHANNEL * 2) + out][sample] += m_total_output[out] >> 39;

		m_keyon_ex = 0;
	}
}

u64 jkms8w128e_device::qword_r(offs_t offset)
{
	u16 ret = 0;
	channel_t *chan = &m_channel[m_chan_offs & 0x7f];
	switch (offset & 31)
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
			ret = (chan->eg.delay_time << 32) | chan->eg.delay_step;
			break;
		case 5:
			ret = (chan->eg.attack_time << 32) | chan->eg.attack_step;
			break;
		case 6:
			ret = (chan->eg.hold_time << 32) | chan->eg.hold_step;
			break;
		case 7:
			ret = (chan->eg.decay_time << 32) | chan->eg.decay_step;
			break;
		case 8:
			ret = (chan->eg.sustain_time << 32) | chan->eg.sustain_step;
			break;
		case 9:
			ret = (chan->eg.init_level << 32) | chan->eg.release_step;
			break;
		case 10:
			ret = (chan->eg.delay_add << 32) | chan->eg.attack_add;
			break;
		case 11:
			ret = (chan->eg.hold_add << 32) | chan->eg.decay_add;
			break;
		case 12:
			ret = chan->eg.sustain_add << 32;
			break;
		case 13:
			ret = (chan->weg.offs << 48) | (chan->weg.len << 32) | (chan->weg.freq << 16) | chan->weg.ctrl;
			break;
		case 14:
		case 15:
			ret = (m_wave_offs << 48) | (m_waveform[m_wave_offs & WAVE_MASK] << 32) | (chan->mvol << 16) | (m_keyon_ex << 8) | m_chan_offs;
			break;
		case 16:
		case 20:
		case 24:
		case 28:
			return chan->output[0];
		case 17:
		case 21:
		case 25:
		case 29:
			return chan->output[1];
		case 18:
		case 22:
		case 26:
		case 30:
			return m_total_output[0];
		case 19:
		case 23:
		case 27:
		case 31:
			return m_total_output[1];
	}
	return ret;
}

/*
	Register Map (64bit)

		fedcba98 76543210 fedcba98 76543210 fedcba98 76543210 fedcba98 76543210
	0	ffffffff ffffffff ffffffff ffffffff -------- -------- -------- -------- Frequency (f * (clock / 128 / 1048576))
		-------- -------- -------- -------- pppppppp pppppppp -------- -------- FM Stack output level
		-------- -------- -------- -------- -------- -------- e------- -------- Keyon execute bit
		-------- -------- -------- -------- -------- -------- -p------ -------- Pause execute bit
		-------- -------- -------- -------- -------- -------- --w----- -------- Wave Envelope Enable
		-------- -------- -------- -------- -------- -------- ---e---- -------- Envelope Enable
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
		-------- -------- llllllll llllllll -------- -------- -------- -------- Wave length
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
	4	dddddddd dddddddd dddddddd dddddddd -------- -------- -------- -------- Delay Time (d * (clock / 128 / 1048576))
		-------- -------- -------- -------- dddddddd dddddddd dddddddd dddddddd Delay Step (32bit signed)
	5	dddddddd dddddddd dddddddd dddddddd -------- -------- -------- -------- Attack Time (d * (clock / 128 / 1048576))
		-------- -------- -------- -------- dddddddd dddddddd dddddddd dddddddd Attack Step (32bit signed)
	6	dddddddd dddddddd dddddddd dddddddd -------- -------- -------- -------- Hold Time (d * (clock / 128 / 1048576))
		-------- -------- -------- -------- dddddddd dddddddd dddddddd dddddddd Hold Step (32bit signed)
	7	dddddddd dddddddd dddddddd dddddddd -------- -------- -------- -------- Decay Time (d * (clock / 128 / 1048576))
		-------- -------- -------- -------- dddddddd dddddddd dddddddd dddddddd Decay Step (32bit signed)
	8	dddddddd dddddddd dddddddd dddddddd -------- -------- -------- -------- Sustain Time (d * (clock / 128 / 1048576))
		-------- -------- -------- -------- dddddddd dddddddd dddddddd dddddddd Sustain Step (32bit signed)
	9	iiiiiiii iiiiiiii iiiiiiii iiiiiiii -------- -------- -------- -------- Initial envelope level
		-------- -------- -------- -------- dddddddd dddddddd dddddddd dddddddd Release Step (32bit unsigned)
	a	dddddddd dddddddd dddddddd dddddddd -------- -------- -------- -------- Delay Step add value (32bit signed)
		-------- -------- -------- -------- dddddddd dddddddd dddddddd dddddddd Attack Step add value (32bit signed)
	b	dddddddd dddddddd dddddddd dddddddd -------- -------- -------- -------- Hold Step add value (32bit signed)
		-------- -------- -------- -------- dddddddd dddddddd dddddddd dddddddd Decay Step add value (32bit signed)
	c	dddddddd dddddddd dddddddd dddddddd -------- -------- -------- -------- Sustain Step add value (32bit signed)
	d	ffffffff ffffffff -------- -------- -------- -------- -------- -------- Wave Envelope Offset
		-------- -------- ffffffff ffffffff -------- -------- -------- -------- Wave Envelope Length + 1
		-------- -------- -------- -------- ffffffff ffffffff -------- -------- Wave Envelope Frequency (clock / 128 / (f + 1))
		-------- -------- -------- -------- -------- -------- --w----- -------- Wave Envelope mode (Signed or Unsigned)
		-------- -------- -------- -------- -------- -------- ---e---- -------- Wave Envelope Loop
		-------- -------- -------- -------- -------- -------- ----w--- -------- Wave Envelope Invert waveform
		-------- -------- -------- -------- -------- -------- -----a-- -------- Wave Envelope Invert waveaddr
		-------- -------- -------- -------- -------- -------- ------tt -------- Wave Envelope Hold timing
		-------- -------- -------- -------- -------- -------- -------- -p------ Wave Envelope Pause
		-------- -------- -------- -------- -------- -------- -------- --p----- Wave Envelope Waveform pingpong
		-------- -------- -------- -------- -------- -------- -------- ---p---- Wave Envelope Waveaddr pingpong
		-------- -------- -------- -------- -------- -------- -------- ----i--- Wave Envelope Invert pingpong
		-------- -------- -------- -------- -------- -------- -------- -----mmm Wave Envelope Pingpong mode
	e*	wwwwwwww wwwwwwww -------- -------- -------- -------- -------- -------- Waveram Address
		-------- -------- dddddddd dddddddd -------- -------- -------- -------- Waveram data
		-------- -------- -------- -------- mmmmmmmm mmmmmmmm -------- -------- Master volume
		-------- -------- -------- -------- -------- -------- ------e- -------- Execute keyon
		-------- -------- -------- -------- -------- -------- -------p -------- Execute pause
		-------- -------- -------- -------- -------- -------- -------- -sssssss Channel select
		* : 111x (x : don't care)
*/

void jkms8w128e_device::qword_w(offs_t offset, u64 data, u64 mem_mask)
{
	channel_t *chan = &m_channel[m_chan_offs & 0x7f];
	switch (offset & 31)
	{
		case 0:
			if (ACCESSING_BITS_0_15)
			{
				const u16 old_ctrl = chan->ctrl;
				chan->ctrl = ((chan->ctrl & ~mem_mask) | (data & mem_mask)) & 0xffff;
				chan->wave_xor = BIT(chan->ctrl, 11) ? 0xffff : 0;
				chan->reverse = BIT(chan->ctrl, 10) ? 1 : 0;
				if (((chan->ctrl & 0x2000) == 0x2000) && ((old_ctrl & 0x2000) == 0))
				{
					if (!chan->weg.enable)
						chan->weg.keyon();
				}
				if (((chan->ctrl & 0x1000) == 0) && ((old_ctrl & 0x1000) == 0x1000))
				{
					if (chan->eg.enable)
						chan->keyoff();
				}
				chan->weg.enable = BIT(chan->ctrl, 13);
				chan->eg.enable = BIT(chan->ctrl, 12);
				if (((chan->ctrl & 0x80) == 0x80) && ((old_ctrl & 0x80) == 0))
					chan->keyon();
				else if (((chan->ctrl & 0x80) == 0) && ((old_ctrl & 0x80) == 0x80))
				{
					if (!chan->eg.enable)
						chan->keyoff();
					else
						chan->eg.keyoff();
				}
			}
			if (ACCESSING_BITS_16_31)
			{
				chan->tl = ((chan->tl & ~(mem_mask >> 16)) | ((data & mem_mask) >> 16)) & 0xffff;
			}
			if (ACCESSING_BITS_32_63)
			{
				data = (data >> 32) & 0xffffffff;
				mem_mask = (mem_mask >> 32) & 0xffffffff;
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
			if (ACCESSING_BITS_32_63)
				chan->eg.delay_time = (chan->eg.delay_time & ~(mem_mask >> 32)) | ((data & mem_mask) >> 32) & 0xffffffff;
			if (ACCESSING_BITS_0_31)
				chan->eg.delay_step = (chan->eg.delay_step & ~(mem_mask >> 32)) | ((data & mem_mask) >> 32) & 0xffffffff;
			break;
		case 5:
			if (ACCESSING_BITS_32_63)
				chan->eg.attack_time = (chan->eg.attack_time & ~(mem_mask >> 32)) | ((data & mem_mask) >> 32) & 0xffffffff;
			if (ACCESSING_BITS_0_31)
				chan->eg.attack_step = (chan->eg.attack_step & ~(mem_mask >> 32)) | ((data & mem_mask) >> 32) & 0xffffffff;
			break;
		case 6:
			if (ACCESSING_BITS_32_63)
				chan->eg.hold_time = (chan->eg.hold_time & ~(mem_mask >> 32)) | ((data & mem_mask) >> 32) & 0xffffffff;
			if (ACCESSING_BITS_0_31)
				chan->eg.hold_step = (chan->eg.hold_step & ~(mem_mask >> 32)) | ((data & mem_mask) >> 32) & 0xffffffff;
			break;
		case 7:
			if (ACCESSING_BITS_32_63)
				chan->eg.decay_time = (chan->eg.decay_time & ~(mem_mask >> 32)) | ((data & mem_mask) >> 32) & 0xffffffff;
			if (ACCESSING_BITS_0_31)
				chan->eg.decay_step = (chan->eg.decay_step & ~(mem_mask >> 32)) | ((data & mem_mask) >> 32) & 0xffffffff;
			break;
		case 8:
			if (ACCESSING_BITS_32_63)
				chan->eg.sustain_time = (chan->eg.sustain_time & ~(mem_mask >> 32)) | ((data & mem_mask) >> 32) & 0xffffffff;
			if (ACCESSING_BITS_0_31)
				chan->eg.sustain_step = (chan->eg.sustain_step & ~(mem_mask >> 32)) | ((data & mem_mask) >> 32) & 0xffffffff;
			break;
		case 9:
			if (ACCESSING_BITS_32_63)
				chan->eg.init_level = (chan->eg.init_level & ~(mem_mask >> 32)) | ((data & mem_mask) >> 32) & 0xffffffff;
			if (ACCESSING_BITS_0_31)
				chan->eg.release_step = (chan->eg.release_step & ~(mem_mask >> 32)) | ((data & mem_mask) >> 32) & 0xffffffff;
			break;
		case 10:
			if (ACCESSING_BITS_32_63)
				chan->eg.delay_add = (chan->eg.delay_add & ~(mem_mask >> 32)) | ((data & mem_mask) >> 32) & 0xffffffff;
			if (ACCESSING_BITS_0_31)
				chan->eg.attack_add = (chan->eg.attack_add & ~(mem_mask >> 32)) | ((data & mem_mask) >> 32) & 0xffffffff;
			break;
		case 11:
			if (ACCESSING_BITS_32_63)
				chan->eg.hold_add = (chan->eg.hold_add & ~(mem_mask >> 32)) | ((data & mem_mask) >> 32) & 0xffffffff;
			if (ACCESSING_BITS_0_31)
				chan->eg.decay_add = (chan->eg.decay_add & ~(mem_mask >> 32)) | ((data & mem_mask) >> 32) & 0xffffffff;
			break;
		case 12:
			if (ACCESSING_BITS_32_63)
				chan->eg.sustain_add = (chan->eg.sustain_add & ~(mem_mask >> 32)) | ((data & mem_mask) >> 32) & 0xffffffff;
			break;
		case 13:
			if (ACCESSING_BITS_48_63)
			{
				chan->weg.offs = ((chan->weg.offs & ~(mem_mask >> 48)) | ((data & mem_mask) >> 48)) & 0xffff;
			}
			if (ACCESSING_BITS_32_47)
			{
				chan->weg.len = ((chan->weg.len & ~(mem_mask >> 32)) | ((data & mem_mask) >> 32)) & 0xffff;
			}
			if (ACCESSING_BITS_16_31)
			{
				chan->weg.freq = ((chan->weg.freq & ~(mem_mask >> 16)) | ((data & mem_mask) >> 16)) & 0xffff;
			}
			if (ACCESSING_BITS_0_15)
			{
				chan->weg.ctrl = ((chan->weg.ctrl & ~(mem_mask >> 16)) | ((data & mem_mask) >> 16)) & 0xffff;
				data = chan->weg.ctrl;
				chan->weg.wave_xor = BIT(data, 11) ? 0xffff : 0;
				chan->weg.reverse = BIT(data, 10) ? 1 : 0;
				chan->weg.hold_timing = (data & 0x300) >> 8;
			}
			break;
		case 14:
		case 15:
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
				m_chan_offs = data & 0x7f;
			}
			break;
	}
}

u32 jkms8w128e_device::dword_be_r(offs_t offset)
{
	const u8 shift = (~offset & 1) << 5;
	return (qword_r(offset >> 1) >> shift) & 0xffffffff;
}

u32 jkms8w128e_device::dword_le_r(offs_t offset)
{
	const u8 shift = (offset & 1) << 5;
	return (qword_r(offset >> 1) >> shift) & 0xffffffff;
}

void jkms8w128e_device::dword_be_w(offs_t offset, u32 data, u32 mem_mask)
{
	const u8 shift = (~offset & 1) << 5;
	qword_w(offset >> 1, data << shift, mem_mask << shift);
}

void jkms8w128e_device::dword_le_w(offs_t offset, u32 data, u32 mem_mask)
{
	const u8 shift = (offset & 1) << 5;
	qword_w(offset >> 1, data << shift, mem_mask << shift);
}

u16 jkms8w128e_device::word_be_r(offs_t offset)
{
	const u8 shift = (~offset & 3) << 4;
	return (qword_r(offset >> 2) >> shift) & 0xffff;
}

u16 jkms8w128e_device::word_le_r(offs_t offset)
{
	const u8 shift = (offset & 3) << 4;
	return (qword_r(offset >> 2) >> shift) & 0xffff;
}

void jkms8w128e_device::word_be_w(offs_t offset, u16 data, u16 mem_mask)
{
	const u8 shift = (~offset & 3) << 4;
	qword_w(offset >> 2, data << shift, mem_mask << shift);
}

void jkms8w128e_device::word_le_w(offs_t offset, u16 data, u16 mem_mask)
{
	const u8 shift = (offset & 3) << 4;
	qword_w(offset >> 2, data << shift, mem_mask << shift);
}

u8 jkms8w128e_device::byte_be_r(offs_t offset)
{
	const u8 shift = (~offset & 7) << 3;
	return (qword_r(offset >> 3) >> shift) & 0xff;
}

u8 jkms8w128e_device::byte_le_r(offs_t offset)
{
	const u8 shift = (offset & 7) << 3;
	return (qword_r(offset >> 3) >> shift) & 0xff;
}

void jkms8w128e_device::byte_be_w(offs_t offset, u8 data, u8 mem_mask)
{
	const u8 shift = (~offset & 7) << 3;
	qword_w(offset >> 3, data << shift, mem_mask << shift);
}

void jkms8w128e_device::byte_le_w(offs_t offset, u8 data, u8 mem_mask)
{
	const u8 shift = (offset & 7) << 3;
	qword_w(offset >> 3, data << shift, mem_mask << shift);
}
