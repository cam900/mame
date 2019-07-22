
// license:BSD-3-Clause
// copyright-holders:MATRIX
/*
	MATRIX JKMS8P64
	8 Channel PCM/ADPCM Player
*/
#include "emu.h"
#include "jkms8p64.h"

DEFINE_DEVICE_TYPE(JKMS8P64, jkms8p64_device, "jkms8p64", "MATRIX JKMS8P64 PCM/ADPCM Player")

jkms8p64_device::jkms8p64_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, JKMS8P64, tag, owner, clock)
	, device_sound_interface(mconfig, *this)
	, device_memory_interface(mconfig, *this)
	, m_stream(nullptr)
	, m_sampledata_config("sample", ENDIANNESS_LITTLE, 8, 32, 0)
	, m_channel_offs(0)
{
}

device_memory_interface::space_config_vector jkms8p64_device::memory_space_config() const
{
	return space_config_vector {
		std::make_pair(0, &m_sampledata_config)
	};
}

void jkms8p64_device::device_start()
{
	m_stream = machine().sound().stream_alloc(*this, 0, 2, clock() / 1024);
	m_sampledata = &space(0);
	for (int ch = 0; ch < MAX_CHANNEL; ch++)
	{
		channel_t *chan = &m_channel[ch];
		save_item(NAME(chan->playing), ch);
		save_item(NAME(chan->looping), ch);
		save_item(NAME(chan->ctrl), ch);
		save_item(NAME(chan->freq), ch);
		save_item(NAME(chan->frac), ch);
		save_item(NAME(chan->update), ch);
		save_item(NAME(chan->start), ch);
		save_item(NAME(chan->loop), ch);
		save_item(NAME(chan->loopend), ch);
		save_item(NAME(chan->end), ch);
		save_item(NAME(chan->pos), ch);
		save_item(NAME(chan->mvol), ch);
		save_item(NAME(chan->lvol), ch);
		save_item(NAME(chan->rvol), ch);
		save_item(NAME(chan->prev_sample), ch);
		save_item(NAME(chan->curr_sample), ch);
		for (int adpcm = 0; adpcm < 2; adpcm++)
		{
			save_item(NAME(chan->adpcm[adpcm].step), (adpcm << 8) | ch);
			save_item(NAME(chan->adpcm[adpcm].signal), (adpcm << 8) | ch);
			save_item(NAME(chan->adpcm[adpcm].loopstep), (adpcm << 8) | ch);
			save_item(NAME(chan->adpcm[adpcm].loopsignal), (adpcm << 8) | ch);
			save_item(NAME(chan->adpcm[adpcm].looped), (adpcm << 8) | ch);
		}
	}
	save_item(NAME(m_channel_offs));
	save_item(NAME(m_keyon_ex));
}

void jkms8p64_device::device_reset()
{
	for (int ch = 0; ch < MAX_CHANNEL; ch++)
	{
		channel_t *chan = &m_channel[ch];
		chan->playing = false;
		chan->looping = false;
		chan->ctrl = 0;
		chan->freq = 0;
		chan->frac = 0;
		chan->update = 0;
		chan->start = 0;
		chan->loop = 0;
		chan->loopend = 0;
		chan->end = 0;
		chan->pos = 0;
		chan->mvol = 0;
		chan->lvol = 0;
		chan->rvol = 0;
		std::fill(std::begin(chan->prev_sample), std::end(chan->prev_sample), 0);
		std::fill(std::begin(chan->curr_sample), std::end(chan->curr_sample), 0);
		for (int adpcm = 0; adpcm < 2; adpcm++)
		{
			chan->adpcm[adpcm].reset();
		}
	}
	m_channel_offs = 0;
	m_keyon_ex = 0;
}

void jkms8p64_device::device_clock_changed()
{
	m_stream->set_sample_rate(clock() / 1024);
}

void jkms8p64_device::sound_stream_update(sound_stream &stream, stream_sample_t **inputs, stream_sample_t **outputs, int samples)
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
				s64 interp_sample[2] = {0,0};
				for (int out = 0; out < 2; out++)
				{
					interp_sample[out] = (((((s32)chan->prev_sample[out] * ((1 << 16) - chan->frac)) + ((s32)chan->curr_sample[out] * chan->frac)) >> 16) * chan->mvol) >> 16;
				}
				interp_sample[0] = (interp_sample[0] * ((chan->ctrl & 0x100) ? -chan->lvol : chan->lvol)) >> 16;
				interp_sample[1] = (interp_sample[1] * ((chan->ctrl & 0x200) ? -chan->rvol : chan->rvol)) >> 16;

				chan->frac += chan->freq;
				chan->update += chan->frac >> 16;
				chan->frac &= 0xffff;
				while (chan->update)
				{
					chan->tick();
					get_sample(chan);
					chan->update--;
					if (!chan->playing)
					{
						chan->update = 0;
						break;
					}
				}

				outputs[0][sample] += interp_sample[0];
				outputs[1][sample] += interp_sample[1];
			}
		}
		m_keyon_ex = 0;
	}
}

u32 jkms8p64_device::dword_r(offs_t offset)
{
	channel_t *chan = &m_channel[m_channel_offs];
	switch (offset)
	{
		case 0:
			return chan->ctrl;
		case 1:
			return chan->freq;
		case 2:
			return chan->start;
		case 3:
			return chan->loop;
		case 4:
			return chan->loopend;
		case 5:
			return chan->end;
		case 6:
			return (chan->lvol << 16) | chan->rvol;
		case 7:
			return (chan->mvol << 16) | m_keyon_ex | m_channel_offs;
	}
}

/*
	Register Map

		fedcba98 76543210 fedcba98 76543210
	0	-------- -------- e------- -------- Keyon execute bit
		-------- -------- -p------ -------- Pause execute bit
		-------- -------- ------r- -------- Invert right output
		-------- -------- -------l -------- Invert left output
		-------- -------- -------- k------- Keyon
		-------- -------- -------- -p------ Pause
		-------- -------- -------- ----l--- Loop enable
		-------- -------- -------- -----s-- Stereo sample
		-------- -------- -------- ------mm Sample format (4bit ADPCM, 8bit PCM, u-Law, 16bit PCM)
	1	-------- ffffffff ffffffff ffffffff Frequency (f * (clock / 1024 / 65536))
	2	ssssssss ssssssss ssssssss ssssssss Start offset
	3	llllllll llllllll llllllll llllllll Loop start
	4	llllllll llllllll llllllll llllllll Loop end
	5	eeeeeeee eeeeeeee eeeeeeee eeeeeeee Length
	6	llllllll llllllll -------- -------- Left volume
		-------- -------- rrrrrrrr rrrrrrrr Right volume
	7	mmmmmmmm mmmmmmmm -------- -------- Master volume
		-------- -------- -------- e------- Execute keyon
		-------- -------- -------- -p------ Execute pause
		-------- -------- -------- -----sss Channel select
*/

void jkms8p64_device::dword_w(offs_t offset, u32 data, u32 mem_mask)
{
	channel_t *chan = &m_channel[m_channel_offs];
	switch (offset)
	{
		case 0:
			u32 old_ctrl = chan->ctrl;
			data = COMBINE_DATA(&chan->ctrl);
			if (old_ctrl != data)
			{
				if (((old_ctrl & 0x80) == 0) && ((data & 0x80) == 0x80))
				{
					chan->keyon();
					get_sample(chan);
				}
				if (((old_ctrl & 0x80) == 0x80) && ((data & 0x80) == 0))
				{
					chan->keyoff();
				}
			}
			break;
		case 1:
			data &= 0xffffff;
			mem_mask &= 0xffffff;
			COMBINE_DATA(&chan->freq);
			break;
		case 2:
			COMBINE_DATA(&chan->start);
			break;
		case 3:
			COMBINE_DATA(&chan->loop);
			break;
		case 4:
			COMBINE_DATA(&chan->loopend);
			break;
		case 5:
			COMBINE_DATA(&chan->end);
			break;
		case 6:
			if (ACCESSING_BITS_16_31)
				chan->lvol = (chan->lvol & ~(mem_mask >> 16)) | ((data & mem_mask) >> 16);
			if (ACCESSING_BITS_0_15)
				chan->rvol = (chan->rvol & ~mem_mask) | (data & mem_mask);
			break;
		case 7:
			if (ACCESSING_BITS_0_7)
			{
				m_keyon_ex = data & 0xc0;
				m_channel_offs = data & 0x3f;
			}
			if (ACCESSING_BITS_16_31)
			{
				data >>= 16;
				mem_mask >>= 16;
				COMBINE_DATA(&chan->mvol);
			}
			break;
	}
}

u16 jkms8p64_device::word_be_r(offs_t offset)
{
	const u8 shift = (~offset & 1) << 4;
	return (dword_r(offset >> 1) >> shift) & 0xffff;
}

u16 jkms8p64_device::word_le_r(offs_t offset)
{
	const u8 shift = (offset & 1) << 4;
	return (dword_r(offset >> 1) >> shift) & 0xffff;
}

void jkms8p64_device::word_be_w(offs_t offset, u16 data, u16 mem_mask)
{
	const u8 shift = (~offset & 1) << 4;
	dword_w(offset >> 1, data << shift, mem_mask << shift);
}

void jkms8p64_device::word_le_w(offs_t offset, u16 data, u16 mem_mask)
{
	const u8 shift = (offset & 1) << 4;
	dword_w(offset >> 1, data << shift, mem_mask << shift);
}

u8 jkms8p64_device::byte_be_r(offs_t offset)
{
	const u8 shift = (~offset & 3) << 3;
	return (dword_r(offset >> 2) >> shift) & 0xff;
}

u8 jkms8p64_device::byte_le_r(offs_t offset)
{
	const u8 shift = (offset & 3) << 3;
	return (dword_r(offset >> 2) >> shift) & 0xff;
}

void jkms8p64_device::byte_be_w(offs_t offset, u8 data, u8 mem_mask)
{
	const u8 shift = (~offset & 3) << 3;
	dword_w(offset >> 2, data << shift, mem_mask << shift);
}

void jkms8p64_device::byte_le_w(offs_t offset, u8 data, u8 mem_mask)
{
	const u8 shift = (offset & 3) << 3;
	dword_w(offset >> 2, data << shift, mem_mask << shift);
}
