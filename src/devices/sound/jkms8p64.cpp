
// license:BSD-3-Clause
// copyright-holders:MATRIX
/*
	MATRIX JKMS8P64
	64 Channel PCM/ADPCM Player
	internal 54 bit DAC
	Clock : ~100MHz
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

	//u-Law table as per MIL-STD-188-113
	u16 lut[8];
	const u16 lut_initial = 33 << 2;   //shift up 2-bits for 16-bit range.
	for (int i = 0; i < 8; i++)
		lut[i] = (lut_initial << i) - lut_initial;
	for (int i = 0; i < 256; i++)
	{
		const u8 exponent = (~i >> 4) & 0x07;
		const u8 mantissa = ~i & 0x0f;
		const s16 value = lut[exponent] + (mantissa << (exponent + 3));
		m_mulaw[i] = (i & 0x80) ? -value : value;
	}

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
		save_item(NAME(chan->output), ch);
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
	save_item(NAME(m_total_output));
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
		std::fill(std::begin(m_total_output), std::end(m_total_output), 0);
		for (int ch = 0; ch < MAX_CHANNEL; ch++)
		{
			channel_t *chan = &m_channel[ch];
			std::fill(std::begin(chan->output), std::end(chan->output), 0);
			if (m_keyon_ex & 0x2)
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
			if (m_keyon_ex & 0x1)
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
					interp_sample[out] = ((((s32)chan->prev_sample[out] * ((1 << 16) - chan->frac)) + ((s32)chan->curr_sample[out] * chan->frac)) >> 16) * chan->mvol;
				}
				interp_sample[0] = interp_sample[0] * ((chan->ctrl & 0x100) ? -chan->lvol : chan->lvol);
				interp_sample[1] = interp_sample[1] * ((chan->ctrl & 0x200) ? -chan->rvol : chan->rvol);

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
				for (int out = 0; out < 2; out++)
				{
					chan->output[out] = interp_sample[out];
					m_total_output[out] += chan->output[out];
				}
			}
		}

		outputs[0][sample] += m_total_output[0] >> 38;
		outputs[1][sample] += m_total_output[1] >> 38;

		m_keyon_ex = 0;
	}
}

u64 jkms8p64_device::qword_r(offs_t offset)
{
	channel_t *chan = &m_channel[m_channel_offs];
	switch (offset)
	{
		case 0:
			return (chan->freq << 32) | chan->ctrl;
		case 1:
			return (chan->start << 32) | chan->loop;
		case 2:
			return (chan->loopend << 32) | chan->end;
		case 3:
			return (chan->lvol << 48) | (chan->rvol << 32) | (chan->mvol << 16) | (m_keyon_ex << 8) | m_channel_offs;
		case 4:
			return chan->output[0];
		case 5:
			return chan->output[1];
		case 6:
			return m_total_output[0];
		case 7:
			return m_total_output[1];
	}
}

/*
	Register Map (64bit)

		fedcba98 76543210 fedcba98 76543210 fedcba98 76543210 fedcba98 76543210
	0	-------- ffffffff ffffffff ffffffff -------- -------- -------- -------- Frequency (f * (clock / 1024 / 65536))
		-------- -------- -------- -------- -------- -------- e------- -------- Keyon execute bit
		-------- -------- -------- -------- -------- -------- -p------ -------- Pause execute bit
		-------- -------- -------- -------- -------- -------- ------r- -------- Invert right output
		-------- -------- -------- -------- -------- -------- -------l -------- Invert left output
		-------- -------- -------- -------- -------- -------- -------- k------- Keyon
		-------- -------- -------- -------- -------- -------- -------- -p------ Pause
		-------- -------- -------- -------- -------- -------- -------- ----l--- Loop enable
		-------- -------- -------- -------- -------- -------- -------- -----s-- Stereo sample
		-------- -------- -------- -------- -------- -------- -------- ------mm Sample format (4bit ADPCM, 8bit PCM, u-Law, 16bit PCM)
	1	ssssssss ssssssss ssssssss ssssssss -------- -------- -------- -------- Start offset
		-------- -------- -------- -------- llllllll llllllll llllllll llllllll Loop start
	2	llllllll llllllll llllllll llllllll -------- -------- -------- -------- Loop end
		-------- -------- -------- -------- eeeeeeee eeeeeeee eeeeeeee eeeeeeee Length
	3	llllllll llllllll -------- -------- -------- -------- -------- -------- Left volume
		-------- -------- rrrrrrrr rrrrrrrr -------- -------- -------- -------- Right volume
		-------- -------- -------- -------- mmmmmmmm mmmmmmmm -------- -------- Master volume
		-------- -------- -------- -------- -------- -------- ------e- -------- Execute keyon
		-------- -------- -------- -------- -------- -------- -------p -------- Execute pause
		-------- -------- -------- -------- -------- -------- -------- --ssssss Channel select
*/

void jkms8p64_device::qword_w(offs_t offset, u64 data, u64 mem_mask)
{
	channel_t *chan = &m_channel[m_channel_offs];
	switch (offset)
	{
		case 0:
			if (ACCESSING_BITS_0_31)
			{
				u32 old_ctrl = chan->ctrl;
				chan->ctrl = ((chan->ctrl & ~mem_mask) | (data & mem_mask)) & 0xffffffff;
				data = chan->ctrl;
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
			}
			if (ACCESSING_BITS_32_63)
			{
				data = (data >> 32) & 0xffffff;
				mem_mask = (mem_mask >> 32) & 0xffffff;
				COMBINE_DATA(&chan->freq);
			}
			break;
		case 1:
			if (ACCESSING_BITS_32_63)
			{
				chan->start = ((chan->start & ~(mem_mask >> 32)) | ((data & mem_mask) >> 32)) & 0xffffffff;
			}
			if (ACCESSING_BITS_0_31)
			{
				chan->loop = ((chan->loop & ~mem_mask) | (data & mem_mask)) & 0xffffffff;
			}
			break;
		case 2:
			if (ACCESSING_BITS_32_63)
			{
				chan->loopend = ((chan->loopend & ~(mem_mask >> 32)) | ((data & mem_mask) >> 32)) & 0xffffffff;
			}
			if (ACCESSING_BITS_0_31)
			{
				chan->end = ((chan->end & ~mem_mask) | (data & mem_mask)) & 0xffffffff;
			}
			break;
		case 3:
			if (ACCESSING_BITS_48_63)
				chan->lvol = ((chan->lvol & ~(mem_mask >> 48)) | ((data & mem_mask) >> 48)) & 0xffff;
			if (ACCESSING_BITS_32_47)
				chan->rvol = ((chan->rvol & ~(mem_mask >> 32)) | ((data & mem_mask) >> 32)) & 0xffff;
			if (ACCESSING_BITS_16_31)
				chan->mvol = ((chan->mvol & ~(mem_mask >> 16)) | ((data & mem_mask) >> 16)) & 0xffff;
			if (ACCESSING_BITS_8_15)
			{
				m_keyon_ex = (data >> 8) & 0x3;
			}
			if (ACCESSING_BITS_0_7)
			{
				m_channel_offs = data & 0x3f;
			}
			break;
	}
}

u32 jkms8p64_device::dword_be_r(offs_t offset)
{
	const u8 shift = (~offset & 1) << 5;
	return (qword_r(offset >> 1) >> shift) & 0xffffffff;
}

u32 jkms8p64_device::dword_le_r(offs_t offset)
{
	const u8 shift = (offset & 1) << 5;
	return (qword_r(offset >> 1) >> shift) & 0xffffffff;
}

void jkms8p64_device::dword_be_w(offs_t offset, u32 data, u32 mem_mask)
{
	const u8 shift = (~offset & 1) << 5;
	qword_w(offset >> 1, data << shift, mem_mask << shift);
}

void jkms8p64_device::dword_le_w(offs_t offset, u32 data, u32 mem_mask)
{
	const u8 shift = (offset & 1) << 5;
	qword_w(offset >> 1, data << shift, mem_mask << shift);
}

u16 jkms8p64_device::word_be_r(offs_t offset)
{
	const u8 shift = (~offset & 3) << 4;
	return (qword_r(offset >> 2) >> shift) & 0xffff;
}

u16 jkms8p64_device::word_le_r(offs_t offset)
{
	const u8 shift = (offset & 3) << 4;
	return (qword_r(offset >> 2) >> shift) & 0xffff;
}

void jkms8p64_device::word_be_w(offs_t offset, u16 data, u16 mem_mask)
{
	const u8 shift = (~offset & 3) << 4;
	qword_w(offset >> 2, data << shift, mem_mask << shift);
}

void jkms8p64_device::word_le_w(offs_t offset, u16 data, u16 mem_mask)
{
	const u8 shift = (offset & 3) << 4;
	qword_w(offset >> 2, data << shift, mem_mask << shift);
}

u8 jkms8p64_device::byte_be_r(offs_t offset)
{
	const u8 shift = (~offset & 7) << 3;
	return (qword_r(offset >> 3) >> shift) & 0xff;
}

u8 jkms8p64_device::byte_le_r(offs_t offset)
{
	const u8 shift = (offset & 7) << 3;
	return (qword_r(offset >> 3) >> shift) & 0xff;
}

void jkms8p64_device::byte_be_w(offs_t offset, u8 data, u8 mem_mask)
{
	const u8 shift = (~offset & 7) << 3;
	qword_w(offset >> 3, data << shift, mem_mask << shift);
}

void jkms8p64_device::byte_le_w(offs_t offset, u8 data, u8 mem_mask)
{
	const u8 shift = (offset & 7) << 3;
	qword_w(offset >> 3, data << shift, mem_mask << shift);
}
