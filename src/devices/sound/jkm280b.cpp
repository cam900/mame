// license:BSD-3-Clause
// copyright-holders:Aaron Giles
/**********************************************************************************************
 *
 *   MATRIX JKM280B driver
 *   
 *   YMZ280B with independent 16 bit left/right volume, 16 bit registers, 32 bit address,
 *   16 voices, Stereo sample support
 *
 **********************************************************************************************/


#include "emu.h"
#include "jkm280b.h"

#if JKM280B_MAKE_WAVS
#include "sound/wavwrite.h"
#endif


#define FRAC_BITS           24
#define FRAC_ONE            (1 << FRAC_BITS)
#define FRAC_MASK           (FRAC_ONE - 1)

#define MAX_SAMPLE_CHUNK    FRAC_ONE

#define INTERNAL_BUFFER_SIZE    (1 << 25)
#define INTERNAL_SAMPLE_RATE    (m_master_clock * 2.0)



/* step size index shift table */
static constexpr int index_scale[8] = { 0x0e6, 0x0e6, 0x0e6, 0x0e6, 0x133, 0x199, 0x200, 0x266 };

/* lookup table for the precomputed difference */
static int diff_lookup[16];


void jkm280b_device::update_irq_state()
{
	u16 irq_bits = m_status_register & m_irq_mask;

	/* always off if the enable is off */
	if (!m_irq_enable)
		irq_bits = 0;

	/* update the state if changed */
	if (irq_bits && !m_irq_state)
	{
		m_irq_state = 1;
		if (!m_irq_handler.isnull())
			m_irq_handler(1);
		else logerror("JKM280B: IRQ generated, but no callback specified!\n");
	}
	else if (!irq_bits && m_irq_state)
	{
		m_irq_state = 0;
		if (!m_irq_handler.isnull())
			m_irq_handler(0);
		else logerror("JKM280B: IRQ generated, but no callback specified!\n");
	}
}


void jkm280b_device::update_step(struct JKM280BVoice *voice)
{
	double frequency;

	/* compute the frequency */
	frequency = m_master_clock * (double)(voice->fnum & 0xffffff) * (1.0 / 65536.0);
	voice->output_step = (uint32_t)(frequency * (double)FRAC_ONE / INTERNAL_SAMPLE_RATE);
}


void jkm280b_device::device_post_load()
{
	for (auto & elem : m_voice)
	{
		struct JKM280BVoice *voice = &elem;
		update_step(voice);
		if(voice->irq_schedule)
			voice->timer->adjust(attotime::zero);
	}
}


void jkm280b_device::update_irq_state_timer_common(int voicenum)
{
	struct JKM280BVoice *voice = &m_voice[voicenum];

	if(!voice->irq_schedule) return;

	voice->playing = 0;
	m_status_register |= 1 << voicenum;
	update_irq_state();
	voice->irq_schedule = 0;
}

/**********************************************************************************************

     compute_tables -- compute the difference tables

***********************************************************************************************/

static void compute_tables()
{
	/* loop over all nibbles and compute the difference */
	for (int nib = 0; nib < 16; nib++)
	{
		int value = (nib & 0x07) * 2 + 1;
		diff_lookup[nib] = (nib & 0x08) ? -value : value;
	}
}



/**********************************************************************************************

     generate_adpcm -- general ADPCM decoding routine

***********************************************************************************************/

int jkm280b_device::generate_adpcm(struct JKM280BVoice *voice, int16_t *lbuffer, int16_t *rbuffer, int samples)
{
	int position = voice->position;
	int signal[2] = { voice->signal[0], voice->signal[1] };
	int step[2] = { voice->step[0], voice->step[1] };
	int val[2];

	/* two cases: first cases is non-looping */
	if (!voice->looping)
	{
		/* loop while we still have samples to generate */
		while (samples)
		{
			/* compute the new amplitude and update the current step */
			if (voice->stereo)
			{
				val[0] = read_byte(position / 2) >> ((~position & 1) << 2);
				position++;
				val[1] = read_byte(position / 2) >> ((~position & 1) << 2);
			}
			else
			{
				val[0] = val[1] = read_byte(position / 2) >> ((~position & 1) << 2);
			}
			for (int i = 0; i < 2; i++)
			{
				signal[i] += (step[i] * diff_lookup[val[i] & 15]) / 8;

				/* clamp to the maximum */
				if (signal[i] > 32767)
					signal[i] = 32767;
				else if (signal[i] < -32768)
					signal[i] = -32768;

				/* adjust the step size and clamp */
				step[i] = (step[i] * index_scale[val[i] & 7]) >> 8;
				if (step[i] > 0x6000)
					step[i] = 0x6000;
				else if (step[i] < 0x7f)
					step[i] = 0x7f;
			}

			/* output to the buffer, scaling by the volume */
			*lbuffer++ = signal[0];
			*rbuffer++ = signal[1];
			samples--;

			/* next! */
			position++;
			if (position >= voice->stop)
			{
				voice->ended = true;
				break;
			}
		}
	}

	/* second case: looping */
	else
	{
		/* loop while we still have samples to generate */
		while (samples)
		{
			/* compute the new amplitude and update the current step */
			if (voice->stereo)
			{
				val[0] = read_byte(position / 2) >> ((~position & 1) << 2);
				position++;
				val[1] = read_byte(position / 2) >> ((~position & 1) << 2);
			}
			else
			{
				val[0] = val[1] = read_byte(position / 2) >> ((~position & 1) << 2);
			}
			for (int i = 0; i < 2; i++)
			{
				signal[i] += (step[i] * diff_lookup[val[i] & 15]) / 8;

				/* clamp to the maximum */
				if (signal[i] > 32767)
					signal[i] = 32767;
				else if (signal[i] < -32768)
					signal[i] = -32768;

				/* adjust the step size and clamp */
				step[i] = (step[i] * index_scale[val[i] & 7]) >> 8;
				if (step[i] > 0x6000)
					step[i] = 0x6000;
				else if (step[i] < 0x7f)
					step[i] = 0x7f;
			}

			/* output to the buffer, scaling by the volume */
			*lbuffer++ = signal[0];
			*rbuffer++ = signal[1];
			samples--;

			/* next! */
			position++;
			if (position == voice->loop_start && voice->loop_count == 0)
			{
				for (int i = 0; i < 2; i++)
				{
					voice->loop_signal[i] = signal[i];
					voice->loop_step[i] = step[i];
				}
			}
			if (position >= voice->loop_end)
			{
				if (voice->keyon)
				{
					position = voice->loop_start;
					for (int i = 0; i < 2; i++)
					{
						signal[i] = voice->loop_signal[i];
						step[i] = voice->loop_step[i];
					}
					voice->loop_count++;
				}
			}
			if (position >= voice->stop)
			{
				voice->ended = true;
				break;
			}
		}
	}

	/* update the parameters */
	voice->position = position;
	for (int i = 0; i < 2; i++)
	{
		voice->signal[i] = signal[i];
		voice->step[i] = step[i];
	}

	return samples;
}



/**********************************************************************************************

     generate_pcm8 -- general 8-bit PCM decoding routine

***********************************************************************************************/

int jkm280b_device::generate_pcm8(struct JKM280BVoice *voice, int16_t *lbuffer, int16_t *rbuffer, int samples)
{
	int position = voice->position;
	int val[2];

	/* two cases: first cases is non-looping */
	if (!voice->looping)
	{
		/* loop while we still have samples to generate */
		while (samples)
		{
			/* fetch the current value */
			if (voice->stereo)
			{
				val[0] = read_byte(position / 2);
				position += 2;
				val[1] = read_byte(position / 2);
			}
			else
			{
				val[0] = val[1] = read_byte(position / 2);
			}

			/* output to the buffer, scaling by the volume */
			*lbuffer++ = (int8_t)val[0] * 256;
			*rbuffer++ = (int8_t)val[1] * 256;
			samples--;

			/* next! */
			position += 2;
			if (position >= voice->stop)
			{
				voice->ended = true;
				break;
			}
		}
	}

	/* second case: looping */
	else
	{
		/* loop while we still have samples to generate */
		while (samples)
		{
			/* fetch the current value */
			if (voice->stereo)
			{
				val[0] = read_byte(position / 2);
				position += 2;
				val[1] = read_byte(position / 2);
			}
			else
			{
				val[0] = val[1] = read_byte(position / 2);
			}

			/* output to the buffer, scaling by the volume */
			*lbuffer++ = (int8_t)val[0] * 256;
			*rbuffer++ = (int8_t)val[1] * 256;
			samples--;

			/* next! */
			position += 2;
			if (position >= voice->loop_end)
			{
				if (voice->keyon)
					position = voice->loop_start;
			}
			if (position >= voice->stop)
			{
				voice->ended = true;
				break;
			}
		}
	}

	/* update the parameters */
	voice->position = position;

	return samples;
}



/**********************************************************************************************

     generate_pcm16 -- general 16-bit PCM decoding routine

***********************************************************************************************/

int jkm280b_device::generate_pcm16(struct JKM280BVoice *voice, int16_t *lbuffer, int16_t *rbuffer, int samples)
{
	int position = voice->position;
	int val[2];

	/* two cases: first cases is non-looping */
	if (!voice->looping)
	{
		/* loop while we still have samples to generate */
		while (samples)
		{
			/* fetch the current value */
			if (voice->stereo)
			{
				val[0] = (int16_t)((read_byte(position / 2 + 1) << 8) + read_byte(position / 2 + 0));
				position += 4;
				val[1] = (int16_t)((read_byte(position / 2 + 1) << 8) + read_byte(position / 2 + 0));
			}
			else
			{
				val[0] = val[1] = (int16_t)((read_byte(position / 2 + 1) << 8) + read_byte(position / 2 + 0));
			}

			/* output to the buffer, scaling by the volume */
			*lbuffer++ = val[0];
			*rbuffer++ = val[1];
			samples--;

			/* next! */
			position += 4;
			if (position >= voice->stop)
			{
				voice->ended = true;
				break;
			}
		}
	}

	/* second case: looping */
	else
	{
		/* loop while we still have samples to generate */
		while (samples)
		{
			/* fetch the current value */
			if (voice->stereo)
			{
				val[0] = (int16_t)((read_byte(position / 2 + 1) << 8) + read_byte(position / 2 + 0));
				position += 4;
				val[1] = (int16_t)((read_byte(position / 2 + 1) << 8) + read_byte(position / 2 + 0));
			}
			else
			{
				val[0] = val[1] = (int16_t)((read_byte(position / 2 + 1) << 8) + read_byte(position / 2 + 0));
			}

			/* output to the buffer, scaling by the volume */
			*lbuffer++ = val[0];
			*rbuffer++ = val[1];
			samples--;

			/* next! */
			position += 4;
			if (position >= voice->loop_end)
			{
				if (voice->keyon)
					position = voice->loop_start;
			}
			if (position >= voice->stop)
			{
				voice->ended = true;
				break;
			}
		}
	}

	/* update the parameters */
	voice->position = position;

	return samples;
}




//-------------------------------------------------
//  sound_stream_update - handle a stream update
//-------------------------------------------------

void jkm280b_device::sound_stream_update(sound_stream &stream, stream_sample_t **inputs, stream_sample_t **outputs, int samples)
{
	stream_sample_t *lacc = outputs[0];
	stream_sample_t *racc = outputs[1];
	int v;

	/* clear out the accumulator */
	memset(lacc, 0, samples * sizeof(lacc[0]));
	memset(racc, 0, samples * sizeof(racc[0]));

	/* loop over voices */
	for (v = 0; v < 16; v++)
	{
		struct JKM280BVoice *voice = &m_voice[v];
		int16_t prev[2] = { voice->last_lsample, voice->last_rsample };
		int16_t curr[2] = { voice->curr_lsample, voice->curr_rsample };
		int16_t *curr_ldata = m_scratch[0].get();
		int16_t *curr_rdata = m_scratch[1].get();
		int32_t *ldest = lacc;
		int32_t *rdest = racc;
		uint32_t new_samples, samples_left;
		uint32_t final_pos;
		int remaining = samples;
		int lvol = voice->output_left;
		int rvol = voice->output_right;

		/* quick out if we're not playing and we're at 0 */
		if ((!voice->fnum) || !voice->playing && curr[0] == 0 && curr[1] == 0 && prev[0] == 0 && prev[1] == 0)
		{
			/* make sure next sound plays immediately */
			voice->output_pos = FRAC_ONE;

			continue;
		}

		/* finish off the current sample */
		/* interpolate */
		while (remaining > 0 && voice->output_pos < FRAC_ONE)
		{
			int interp_lsample = (((int32_t)prev[0] * (FRAC_ONE - voice->output_pos)) + ((int32_t)curr[0] * voice->output_pos)) >> FRAC_BITS;
			int interp_rsample = (((int32_t)prev[1] * (FRAC_ONE - voice->output_pos)) + ((int32_t)curr[1] * voice->output_pos)) >> FRAC_BITS;
			*ldest++ += interp_lsample * lvol;
			*rdest++ += interp_rsample * rvol;
			voice->output_pos += voice->output_step;
			remaining--;
		}

		/* if we're over, continue; otherwise, we're done */
		if (voice->output_pos >= FRAC_ONE)
		{
			while (voice->output_pos >= FRAC_ONE)
			{
				voice->output_pos -= FRAC_ONE;
			}
		}
		else
			continue;

		/* compute how many new samples we need */
		final_pos = voice->output_pos + remaining * voice->output_step;
		new_samples = (final_pos + FRAC_ONE) >> FRAC_BITS;
		if (new_samples > MAX_SAMPLE_CHUNK)
			new_samples = MAX_SAMPLE_CHUNK;
		samples_left = new_samples;

		/* generate them into our buffer */
		switch (voice->playing << 7 | voice->mode)
		{
			case 0x81:  samples_left = generate_adpcm(voice, m_scratch[0].get(), m_scratch[1].get(), new_samples); break;
			case 0x82:  samples_left = generate_pcm8(voice, m_scratch[0].get(), m_scratch[1].get(), new_samples); break;
			case 0x83:  samples_left = generate_pcm16(voice, m_scratch[0].get(), m_scratch[1].get(), new_samples); break;
			default:    samples_left = 0; std::fill_n(&m_scratch[0][0], new_samples, 0); std::fill_n(&m_scratch[1][0], new_samples, 0); break;
		}

		if (samples_left || voice->ended)
		{
			voice->ended = false;

			/* if there are leftovers, ramp back to 0 */
			int base = new_samples - samples_left;
			int i, t;
			t = (base == 0) ? curr[0] : m_scratch[0][base - 1];
			for (i = 0; i < samples_left; i++)
			{
				if (t < 0) t = -((-t * 15) >> 4);
				else if (t > 0) t = (t * 15) >> 4;
				m_scratch[0][base + i] = t;
			}
			t = (base == 0) ? curr[1] : m_scratch[1][base - 1];
			for (i = 0; i < samples_left; i++)
			{
				if (t < 0) t = -((-t * 15) >> 4);
				else if (t > 0) t = (t * 15) >> 4;
				m_scratch[1][base + i] = t;
			}

			/* if we hit the end and IRQs are enabled, signal it */
			if (base != 0)
			{
				voice->playing = 0;

				/* set update_irq_state_timer. IRQ is signaled on next CPU execution. */
				voice->timer->adjust(attotime::zero);
				voice->irq_schedule = 1;
			}
		}

		/* advance forward one sample */
		prev[0] = curr[0];
		prev[1] = curr[1];
		curr[0] = *curr_ldata++;
		curr[1] = *curr_rdata++;

		/* then sample-rate convert with linear interpolation */
		while (remaining > 0)
		{
			/* interpolate */
			while (remaining > 0 && voice->output_pos < FRAC_ONE)
			{
				int interp_lsample = (((int32_t)prev[0] * (FRAC_ONE - voice->output_pos)) + ((int32_t)curr[0] * voice->output_pos)) >> FRAC_BITS;
				int interp_rsample = (((int32_t)prev[1] * (FRAC_ONE - voice->output_pos)) + ((int32_t)curr[1] * voice->output_pos)) >> FRAC_BITS;
				*ldest++ += interp_lsample * lvol;
				*rdest++ += interp_rsample * rvol;
				voice->output_pos += voice->output_step;
				remaining--;
			}

			/* if we're over, grab the next samples */
			if (voice->output_pos >= FRAC_ONE)
			{
				prev[0] = curr[0];
				prev[1] = curr[1];
				while (voice->output_pos >= FRAC_ONE)
				{
					curr[0] = *curr_ldata++;
					curr[1] = *curr_rdata++;
					voice->output_pos -= FRAC_ONE;
				}
			}
		}

		/* remember the last samples */
		voice->last_lsample = prev[0];
		voice->last_rsample = prev[1];
		voice->curr_lsample = curr[0];
		voice->curr_rsample = curr[1];
	}

	for (v = 0; v < samples; v++)
	{
		outputs[0][v] /= 65536;
		outputs[1][v] /= 65536;
	}
}



//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void jkm280b_device::device_start()
{
	/* compute ADPCM tables */
	compute_tables();

	/* initialize the rest of the structure */
	m_master_clock = (double)clock() / 384.0;
	m_irq_handler.resolve();

	for (int i = 0; i < 16; i++)
	{
		m_voice[i].timer = timer_alloc(i);
	}

	/* create the stream */
	m_stream = machine().sound().stream_alloc(*this, 0, 2, INTERNAL_SAMPLE_RATE);

	/* allocate memory */
	assert(MAX_SAMPLE_CHUNK < 0x10000);
	m_scratch[0] = std::make_unique<int16_t[]>(MAX_SAMPLE_CHUNK);
	m_scratch[1] = std::make_unique<int16_t[]>(MAX_SAMPLE_CHUNK);

	/* state save */
	save_item(NAME(m_current_register));
	save_item(NAME(m_status_register));
	save_item(NAME(m_irq_state));
	save_item(NAME(m_irq_mask));
	save_item(NAME(m_irq_enable));
	save_item(NAME(m_keyon_enable));
	save_item(NAME(m_ext_mem_enable));
	save_item(NAME(m_ext_mem_address));
	save_item(NAME(m_ext_readlatch));
	save_item(NAME(m_ext_mem_address_hi));
	for (int j = 0; j < 16; j++)
	{
		save_item(NAME(m_voice[j].playing), j);
		save_item(NAME(m_voice[j].ended), j);
		save_item(NAME(m_voice[j].keyon), j);
		save_item(NAME(m_voice[j].looping), j);
		save_item(NAME(m_voice[j].mode), j);
		save_item(NAME(m_voice[j].fnum), j);
		save_item(NAME(m_voice[j].start), j);
		save_item(NAME(m_voice[j].stop), j);
		save_item(NAME(m_voice[j].loop_start), j);
		save_item(NAME(m_voice[j].loop_end), j);
		save_item(NAME(m_voice[j].position), j);
		save_item(NAME(m_voice[j].signal), j);
		save_item(NAME(m_voice[j].step), j);
		save_item(NAME(m_voice[j].loop_signal), j);
		save_item(NAME(m_voice[j].loop_step), j);
		save_item(NAME(m_voice[j].loop_count), j);
		save_item(NAME(m_voice[j].output_left), j);
		save_item(NAME(m_voice[j].output_right), j);
		save_item(NAME(m_voice[j].output_pos), j);
		save_item(NAME(m_voice[j].last_lsample), j);
		save_item(NAME(m_voice[j].curr_lsample), j);
		save_item(NAME(m_voice[j].irq_schedule), j);
	}

#if JKM280B_MAKE_WAVS
	m_wavresample = wav_open("resamp.wav", INTERNAL_SAMPLE_RATE, 2);
#endif
}

//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void jkm280b_device::device_reset()
{
	/* initial clear registers */
	for (int i = 0xff; i >= 0; i--)
	{
		m_current_register = i;
		write_to_register(0, ~0);
	}

	m_current_register = 0;
	m_status_register = 0;
	m_ext_mem_address = 0;

	/* clear other voice parameters */
	for (auto &elem : m_voice)
	{
		struct JKM280BVoice *voice = &elem;

		voice->curr_lsample = 0;
		voice->curr_rsample = 0;
		voice->last_lsample = 0;
		voice->last_rsample = 0;
		voice->output_pos = FRAC_ONE;
		voice->playing = 0;
	}
}


void jkm280b_device::device_timer(emu_timer &timer, device_timer_id id, int param, void *ptr)
{
	if (id < 16)
		update_irq_state_timer_common( id );
	else
		throw emu_fatalerror("Unknown id in jkm280b_device::device_timer");
}


void jkm280b_device::device_clock_changed()
{
	m_master_clock = (double)clock() / 384.0;
	m_stream->set_sample_rate(INTERNAL_SAMPLE_RATE);
}


void jkm280b_device::rom_bank_updated()
{
	m_stream->update();
}


/**********************************************************************************************

     write_to_register -- handle a write to the current register

***********************************************************************************************/

void jkm280b_device::write_to_register(u16 data, u16 mem_mask)
{
	struct JKM280BVoice *voice;
	int i;

	/* lower registers follow a pattern */
	if (m_current_register < 0x80)
	{
		voice = &m_voice[(m_current_register >> 2) & 0xf];

		switch (m_current_register & 0xe3)
		{
			case 0x00:      /* pitch low 8 bits */
				if (ACCESSING_BITS_0_7)
					voice->fnum = (voice->fnum & 0xffff00) | (data & 0x00ff);
				if (ACCESSING_BITS_8_15)
					voice->fnum = (voice->fnum & 0xff00ff) | (data & 0xff00);

				update_step(voice);
				break;

			case 0x01:      /* pitch upper 1 bit, loop, key on, mode */
				if (ACCESSING_BITS_0_7)
				{
					voice->fnum = (voice->fnum & 0xffff) | ((data & 0x00ff) << 16);
					update_step(voice);
				}
				if (ACCESSING_BITS_8_15)
				{
					voice->looping = (data & 0x1000) >> 12;
					if ((data & 0x6000) == 0) data &= 0x7fff; /* ignore mode setting and set to same state as KON=0 */
					else voice->mode = (data & 0x6000) >> 13;
					if (!voice->keyon && (data & 0x8000) && m_keyon_enable)
					{
						voice->playing = 1;
						voice->position = voice->start;
						for (int i = 0; i < 2; i++)
						{
							voice->signal[i] = voice->loop_signal[i] = 0;
							voice->step[i] = voice->loop_step[i] = 0x7f;
						}
						voice->loop_count = 0;

						/* if update_irq_state_timer is set, cancel it. */
						voice->irq_schedule = 0;
					}
					else if (voice->keyon && !(data & 0x8000))
					{
						voice->playing = 0;

						/* if update_irq_state_timer is set, cancel it. */
						voice->irq_schedule = 0;
					}
					voice->keyon = (data & 0x8000) >> 15;
				}
				break;

			case 0x02:      /* total level */
				if (ACCESSING_BITS_0_7)
					voice->output_left = (voice->output_left & 0xff00) | (data & 0x00ff);
				if (ACCESSING_BITS_8_15)
					voice->output_left = (voice->output_left & 0x00ff) | (data & 0xff00);
				break;

			case 0x03:      /* pan */
				if (ACCESSING_BITS_0_7)
					voice->output_right = (voice->output_right & 0xff00) | (data & 0x00ff);
				if (ACCESSING_BITS_8_15)
					voice->output_right = (voice->output_right & 0x00ff) | (data & 0xff00);
				break;

			case 0x40:      /* start address high */
				if (ACCESSING_BITS_0_7)
					voice->start = (voice->start & (0xff00ffff << 1)) | ((data & 0x00ff) << 17);
				if (ACCESSING_BITS_8_15)
					voice->start = (voice->start & (0x00ffffff << 1)) | ((data & 0xff00) << 17);
				break;

			case 0x41:      /* loop start address high */
				if (ACCESSING_BITS_0_7)
					voice->loop_start = (voice->loop_start & (0xff00ffff << 1)) | ((data & 0x00ff) << 17);
				if (ACCESSING_BITS_8_15)
					voice->loop_start = (voice->loop_start & (0x00ffffff << 1)) | ((data & 0xff00) << 17);
				break;

			case 0x42:      /* loop end address high */
				if (ACCESSING_BITS_0_7)
					voice->loop_end = (voice->loop_end & (0xff00ffff << 1)) | ((data & 0x00ff) << 17);
				if (ACCESSING_BITS_8_15)
					voice->loop_end = (voice->loop_end & (0x00ffffff << 1)) | ((data & 0xff00) << 17);
				break;

			case 0x43:      /* stop address high */
				if (ACCESSING_BITS_0_7)
					voice->stop = (voice->stop & (0xff00ffff << 1)) | ((data & 0x00ff) << 17);
				if (ACCESSING_BITS_8_15)
					voice->stop = (voice->stop & (0x00ffffff << 1)) | ((data & 0xff00) << 17);
				break;

			case 0x80:      /* start address middle */
				if (ACCESSING_BITS_0_7)
					voice->start = (voice->start & (0xffffff00 << 1)) | ((data & 0x00ff) << 1);
				if (ACCESSING_BITS_8_15)
					voice->start = (voice->start & (0xffff00ff << 1)) | ((data & 0xff00) << 1);
				break;

			case 0x81:      /* loop start address middle */
				if (ACCESSING_BITS_0_7)
					voice->loop_start = (voice->loop_start & (0xffffff00 << 1)) | ((data & 0x00ff) << 1);
				if (ACCESSING_BITS_8_15)
					voice->loop_start = (voice->loop_start & (0xffff00ff << 1)) | ((data & 0xff00) << 1);
				break;

			case 0x82:      /* loop end address middle */
				if (ACCESSING_BITS_0_7)
					voice->loop_end = (voice->loop_end & (0xffffff00 << 1)) | ((data & 0x00ff) << 1);
				if (ACCESSING_BITS_8_15)
					voice->loop_end = (voice->loop_end & (0xffff00ff << 1)) | ((data & 0xff00) << 1);
				break;

			case 0x83:      /* stop address middle */
				if (ACCESSING_BITS_0_7)
					voice->stop = (voice->stop & (0xffffff00 << 1)) | ((data & 0x00ff) << 1);
				if (ACCESSING_BITS_8_15)
					voice->stop = (voice->stop & (0xffff00ff << 1)) | ((data & 0xff00) << 1);
				break;

			default:
				if (data != 0)
					logerror("JKM280B: unknown register write %02X = %04X\n", m_current_register, data);
				break;
		}
	}

	/* upper registers are special */
	else
	{
		switch (m_current_register)
		{
			/* DSP related (not implemented yet) */
			case 0xc0: // d0-2: DSP Rch, d3: enable Rch (0: yes, 1: no), d4-6: DSP Lch, d7: enable Lch (0: yes, 1: no)
			case 0xc1: // d0: enable control of $82 (0: yes, 1: no)
			case 0xc2: // DSP data
				logerror("JKM280B: DSP register write %02X = %02X\n", m_current_register, data);
				break;

			case 0xc4:      /* ROM readback / RAM write (high) */
				if (ACCESSING_BITS_0_7)
					m_ext_mem_address_hi = (m_ext_mem_address_hi & 0xff00ff00) | ((data & 0x00ff) << 16);
				if (ACCESSING_BITS_8_15)
					m_ext_mem_address_hi = (m_ext_mem_address_hi & 0x00ffff00) | ((data & 0xff00) << 16);
				break;

			case 0xc5:      /* ROM readback / RAM write (low) -> update latch */
				if (ACCESSING_BITS_8_15)
					m_ext_mem_address_hi = (m_ext_mem_address_hi & 0xffff0000) | (data & 0xff00);
				if (ACCESSING_BITS_0_7)
				{
					m_ext_mem_address = m_ext_mem_address_hi | (data & 0x00ff);
					if (m_ext_mem_enable)
						m_ext_readlatch = read_byte(m_ext_mem_address);
				}
				break;

			case 0xc6:      /* RAM write */
				if (ACCESSING_BITS_0_7)
				{
					if (m_ext_mem_enable)
					{
						space(0).write_byte(m_ext_mem_address, data);
						m_ext_mem_address = (m_ext_mem_address + 1) & 0xffffffff;
					}
				}
				break;

			case 0xfe:      /* IRQ mask */
				if (ACCESSING_BITS_0_7)
					m_irq_mask = (m_irq_mask & 0xff00) | (data & 0x00ff);
				if (ACCESSING_BITS_8_15)
					m_irq_mask = (m_irq_mask & 0x00ff) | (data & 0xff00);

				update_irq_state();
				break;

			case 0xff:      /* IRQ enable, test, etc */
				if (ACCESSING_BITS_0_7)
				{
					m_ext_mem_enable = (data & 0x40) >> 6;
					m_irq_enable = (data & 0x10) >> 4;
					update_irq_state();

					if (m_keyon_enable && !(data & 0x80))
					{
						for (i = 0; i < 16; i++)
						{
							m_voice[i].playing = 0;

							/* if update_irq_state_timer is set, cancel it. */
							m_voice[i].irq_schedule = 0;
						}
					}
					else if (!m_keyon_enable && (data & 0x80))
					{
						for (i = 0; i < 16; i++)
						{
							if (m_voice[i].keyon && m_voice[i].looping)
								m_voice[i].playing = 1;
						}
					}
					m_keyon_enable = (data & 0x80) >> 7;
				}
				break;

			default:
				if (data != 0)
					logerror("JKM280B: unknown register write %02X = %04X\n", m_current_register, data);
				break;
		}
	}
}



/**********************************************************************************************

     compute_status -- determine the status bits

***********************************************************************************************/

u16 jkm280b_device::compute_status()
{
	uint16_t result;

	/* force an update */
	m_stream->update();

	result = m_status_register;

	/* clear the IRQ state */
	m_status_register = 0;
	update_irq_state();

	return result;
}



/**********************************************************************************************

     read/write -- handle external accesses

***********************************************************************************************/

u8 jkm280b_device::read8(offs_t offset)
{
	if ((offset & 3) == 1)
	{
		if (!m_ext_mem_enable)
			return 0xff;

		/* read from external memory */
		uint8_t ret = m_ext_readlatch;
		m_ext_readlatch = read_byte(m_ext_mem_address);
		m_ext_mem_address = (m_ext_mem_address + 1) & 0xffffffff;
		return ret;
	}
	else if (offset & 2)
		return (compute_status() >> ((offset & 1) << 3)) & 0xff;

	return 0xff;
}


void jkm280b_device::write8(offs_t offset, u8 data, u8 mem_mask)
{
	if ((offset & 3) == 1)
		COMBINE_DATA(&m_current_register);
	else if (offset & 2)
	{
		const int shift = (offset & 1) << 3;
		/* force an update */
		m_stream->update();

		write_to_register(data << shift, mem_mask << shift);
	}
}

u16 jkm280b_device::read16(offs_t offset, u16 mem_mask)
{
	if ((offset & 1) == 0)
	{
		if (!m_ext_mem_enable)
			return 0xffff;

		/* read from external memory */
		uint8_t ret = 0xff00;
		if (ACCESSING_BITS_0_7)
		{
			ret |= m_ext_readlatch;
			m_ext_readlatch = read_byte(m_ext_mem_address);
			m_ext_mem_address = (m_ext_mem_address + 1) & 0xffffffff;
		}
		return ret;
	}
	else if (offset & 1)
		return compute_status();

	return 0xffff;
}


void jkm280b_device::write16(offs_t offset, u16 data, u16 mem_mask)
{
	if ((offset & 1) == 0)
	{
		if (ACCESSING_BITS_0_7)
			m_current_register = data & 0xff;
	}
	else if (offset & 1)
	{
		/* force an update */
		m_stream->update();

		write_to_register(data, mem_mask);
	}
}


DEFINE_DEVICE_TYPE(JKM280B, jkm280b_device, "jkm280b", "MATRIX JKM280B PCMD8")

jkm280b_device::jkm280b_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, JKM280B, tag, owner, clock)
	, device_sound_interface(mconfig, *this)
	, device_rom_interface(mconfig, *this, 32)
	, m_current_register(0)
	, m_status_register(0)
	, m_irq_state(0)
	, m_irq_mask(0)
	, m_irq_enable(0)
	, m_keyon_enable(0)
	, m_ext_mem_enable(0)
	, m_ext_readlatch(0)
	, m_ext_mem_address_hi(0)
	, m_ext_mem_address(0)
	, m_irq_handler(*this)
{
	memset(m_voice, 0, sizeof(m_voice));
}
