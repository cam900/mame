// license:BSD-3-Clause
// copyright-holders:Aaron Giles
/**********************************************************************************************

	MATRIX JKM5506 OTTO4
	ES5506 with 64 voices, 50 bit sample address bit (1PB samples), Stereo / Invert output
	Operation clock ~100MHz, output rate clock / (16 * active voice)
	65.536KHz at 67.108864MHz, 64 voice
	frequency : output rate / 65536

***********************************************************************************************/

#include "emu.h"
#include "jkm5506.h"

#if JKM5506_MAKE_WAVS
#include "sound/wavwrite.h"
#endif


/**********************************************************************************************

     CONSTANTS

***********************************************************************************************/

#define VERBOSE                 0
#include "logmacro.h"

#define MAX_SAMPLE_CHUNK        10000
#define ULAW_MAXBITS            8

#define CONTROL_ROR             0xe00000
#define CONTROL_BS2             0x100000
#define CONTROL_BS1             0x80000
#define CONTROL_BS0             0x40000
#define CONTROL_INVR            0x20000
#define CONTROL_INVL            0x10000
#define CONTROL_STEREO          0x8000
#define CONTROL_8BIT            0x4000
#define CONTROL_CMPD            0x2000
#define CONTROL_CA2             0x1000
#define CONTROL_CA1             0x0800
#define CONTROL_CA0             0x0400
#define CONTROL_LP4             0x0200
#define CONTROL_LP3             0x0100
#define CONTROL_IRQ             0x0080
#define CONTROL_DIR             0x0040
#define CONTROL_IRQE            0x0020
#define CONTROL_BLE             0x0010
#define CONTROL_LPE             0x0008
#define CONTROL_LEI             0x0004
#define CONTROL_STOP1           0x0002
#define CONTROL_STOP0           0x0001

#define CONTROL_FORMATMASK      (CONTROL_CMPD | CONTROL_8BIT | CONTROL_STEREO)
#define CONTROL_BSMASK          (CONTROL_BS2 | CONTROL_BS1 | CONTROL_BS0)
#define CONTROL_CAMASK          (CONTROL_CA2 | CONTROL_CA1 | CONTROL_CA0)
#define CONTROL_LPMASK          (CONTROL_LP4 | CONTROL_LP3)
#define CONTROL_LOOPMASK        (CONTROL_BLE | CONTROL_LPE)
#define CONTROL_STOPMASK        (CONTROL_STOP1 | CONTROL_STOP0)

DEFINE_DEVICE_TYPE(JKM5506, jkm5506_device, "jkm5506", "MATRIX JKM5506")

jkm5506_device::jkm5506_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock)
	: device_t(mconfig, JKM5506, tag, owner, clock)
	, device_sound_interface(mconfig, *this)
	, device_rom_interface(mconfig, *this, 32, ENDIANNESS_BIG, 32) // actually 1PB
	, m_stream(nullptr)
	, m_sample_rate(0)
	, m_write_latch(0)
	, m_read_latch(0)
	, m_master_clock(0)
	, m_voice_bank(0)
	, m_current_page(0)
	, m_active_voices(0)
	, m_mode(0)
	, m_wst(0)
	, m_wend(0)
	, m_lrend(0)
	, m_irqv(0)
	, m_scratch(nullptr)
	, m_ulaw_lookup(nullptr)
#if JKM5506_MAKE_WAVS
	, m_wavraw(nullptr)
#endif
	, m_channels(0)
	, m_irq_cb(*this)
	, m_read_port_cb(*this)
	, m_sample_rate_changed_cb(*this)
{
}

//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------
void jkm5506_device::device_start()
{
	int j;
	u64 accum_mask;
	int channels = 1;  /* 1 channel by default, for backward compatibility */

	/* only override the number of channels if the value is in the valid range 1 .. 6 */
	if (1 <= m_channels && m_channels <= 6)
		channels = m_channels;

	/* create the stream */
	m_stream = machine().sound().stream_alloc(*this, 0, 2 * channels, clock() / (16*64));

	/* initialize the rest of the structure */
	m_master_clock = clock();
	m_irq_cb.resolve();
	m_read_port_cb.resolve();
	m_sample_rate_changed_cb.resolve();
	m_irqv = 0x80;
	m_channels = channels;

	/* KT-76 assumes all voices are active on an JKM5506 without setting them! */
	m_active_voices = 63;
	m_sample_rate = m_master_clock / (16 * (m_active_voices + 1));
	m_stream->set_sample_rate(m_sample_rate);

	/* compute the tables */
	compute_tables();

	/* init the voices */
	accum_mask = 0xffffffffffffffff;
	for (j = 0; j < 64; j++)
	{
		m_voice[j].index = j;
		m_voice[j].control = CONTROL_STOPMASK;
		m_voice[j].lvol = 0;
		m_voice[j].rvol = 0;
		m_voice[j].accum_mask = accum_mask;
	}

	/* allocate memory */
	m_scratch = make_unique_clear<s32[]>(2 * MAX_SAMPLE_CHUNK);

	/* register save */
	save_item(NAME(m_sample_rate));
	save_item(NAME(m_write_latch));
	save_item(NAME(m_read_latch));

	save_item(NAME(m_voice_bank));
	save_item(NAME(m_current_page));
	save_item(NAME(m_active_voices));
	save_item(NAME(m_mode));
	save_item(NAME(m_wst));
	save_item(NAME(m_wend));
	save_item(NAME(m_lrend));
	save_item(NAME(m_irqv));

	save_pointer(NAME(m_scratch), 2 * MAX_SAMPLE_CHUNK);

	for (j = 0; j < 64; j++)
	{
		save_item(NAME(m_voice[j].control), j);
		save_item(NAME(m_voice[j].freqcount), j);
		save_item(NAME(m_voice[j].start), j);
		save_item(NAME(m_voice[j].lvol), j);
		save_item(NAME(m_voice[j].end), j);
		save_item(NAME(m_voice[j].lvramp), j);
		save_item(NAME(m_voice[j].accum), j);
		save_item(NAME(m_voice[j].rvol), j);
		save_item(NAME(m_voice[j].rvramp), j);
		save_item(NAME(m_voice[j].ecount), j);
		save_item(NAME(m_voice[j].k2), j);
		save_item(NAME(m_voice[j].k2ramp), j);
		save_item(NAME(m_voice[j].k1), j);
		save_item(NAME(m_voice[j].k1ramp), j);
		save_item(NAME(m_voice[j].o4n1), j);
		save_item(NAME(m_voice[j].o3n1), j);
		save_item(NAME(m_voice[j].o3n2), j);
		save_item(NAME(m_voice[j].o2n1), j);
		save_item(NAME(m_voice[j].o2n2), j);
		save_item(NAME(m_voice[j].o1n1), j);
		save_item(NAME(m_voice[j].filtcount), j);
	}

	/* success */
}

//-------------------------------------------------
//  device_clock_changed
//-------------------------------------------------

void jkm5506_device::device_clock_changed()
{
	m_master_clock = clock();
	m_sample_rate = m_master_clock / (16 * (m_active_voices + 1));
	m_stream->set_sample_rate(m_sample_rate);
	if (!m_sample_rate_changed_cb.isnull())
		m_sample_rate_changed_cb(m_sample_rate);
}

//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void jkm5506_device::device_reset()
{
}

//-------------------------------------------------
//  device_stop - device-specific stop
//-------------------------------------------------

void jkm5506_device::device_stop()
{
#if JKM5506_MAKE_WAVS
	{
		int i;

		for (i = 0; i < MAX_JKM5506; i++)
		{
			if (jkm5506[i].m_wavraw)
				wav_close(jkm5506[i].m_wavraw);
		}
	}
#endif
}

//-------------------------------------------------
//  rom_bank_updated - the rom bank has changed
//-------------------------------------------------

void jkm5506_device::rom_bank_updated()
{
	m_stream->update();
}

/**********************************************************************************************

     update_irq_state -- update the IRQ state

***********************************************************************************************/


void jkm5506_device::update_irq_state()
{
	/* ES5505/6 irq line has been set high - inform the host */
	if (!m_irq_cb.isnull())
		m_irq_cb(1); /* IRQB set high */
}

void jkm5506_device::update_internal_irq_state()
{
	/*  Host (cpu) has just read the voice interrupt vector (voice IRQ ack).

	    Reset the voice vector to show the IRQB line is low (top bit set).
	    If we have any stacked interrupts (other voices waiting to be
	    processed - with their IRQ bit set) then they will be moved into
	    the vector next time the voice is processed.  In emulation
	    terms they get updated next time generate_samples() is called.
	*/

	m_irqv=0x80;

	if (!m_irq_cb.isnull())
		m_irq_cb(0); /* IRQB set low */
}

/**********************************************************************************************

     compute_tables -- compute static tables

***********************************************************************************************/

void jkm5506_device::compute_tables()
{
	/* allocate ulaw lookup table */
	m_ulaw_lookup = make_unique_clear<s16[]>(1 << ULAW_MAXBITS);

	//u-Law table as per MIL-STD-188-113
	u16 lut[8];
	u16 lut_initial = 33 << 2;   //shift up 2-bits for 16-bit range.
	for (int i = 0; i < 8; i++)
		lut[i] = (lut_initial << i) - lut_initial;
	for (int i = 0; i < 256; i++)
	{
		u8 exponent = (~i >> 4) & 0x07;
		u8 mantissa = ~i & 0x0f;
		s16 value = lut[exponent] + (mantissa << (exponent + 3));
		m_ulaw_lookup[i] = (i & 0x80) ? -value : value;
	}
}

/**********************************************************************************************

     sample_endianswap -- endian swap for sample data

***********************************************************************************************/

inline u32 jkm5506_device::sample_endianswap(jkm5506_voice *voice, u32 &data, u32 &mask, u32 &shift, bool &is_ulaw)
{
	if (voice->control & CONTROL_BS0)
	{
		data = ((data & 0x0000ffff) << 16) | ((data & 0xffff0000) >> 16);
	}
	if (voice->control & CONTROL_BS1)
	{
		data = ((data & 0x00ff00ff) << 8) | ((data & 0xff00ff00) >> 8);
	}
	if (voice->control & CONTROL_BS2)
	{
		data = ((data & 0x0f0f0f0f) << 4) | ((data & 0xf0f0f0f0) >> 4);
	}
	const u8 s = (voice->control & CONTROL_ROR) >> (21 - 2);
	data = (data >> s) | u32(data << (32 - s));
	switch (voice->control & CONTROL_FORMATMASK)
	{
		case 0: // 16 bit mono
		default:
			data &= 0xffff;
			mask = 0xffff;
			shift = 0;
			return 0;
		case CONTROL_CMPD: // u-law mono
			is_ulaw = true;
		case CONTROL_8BIT: // 8 bit mono
			data &= 0xff;
			mask = 0xff;
			shift = 8;
			return 0;
		case CONTROL_CMPD | CONTROL_8BIT: // 12 bit mono
			data &= 0xfff;
			mask = 0xfff;
			shift = 4;
			return 0;
		case CONTROL_STEREO: // 16 bit stereo
			data &= 0xffffffff;
			mask = 0xffff;
			shift = 0;
			return 16;
		case CONTROL_CMPD | CONTROL_STEREO: // u-law stereo
			is_ulaw = true;
		case CONTROL_8BIT | CONTROL_STEREO: // 8 bit stereo
			data &= 0xffff;
			mask = 0xff;
			shift = 8;
			return 8;
		case CONTROL_CMPD | CONTROL_8BIT | CONTROL_STEREO: // 12 bit stereo
			data &= 0xffffff;
			mask = 0xfff;
			shift = 4;
			return 12;
	}
}


/**********************************************************************************************

     interpolate -- interpolate between two samples

***********************************************************************************************/

static inline void interpolate(s64 &sample1, s64 sample2, u64 accum)
{
	sample1 = s64((sample1 * (s64)(0x10000 - (accum & 0xffff)) +
					sample2 * (s64)(accum & 0xffff)) >> 16);
}


/**********************************************************************************************

     apply_filters -- apply the 4-pole digital filter to the sample

***********************************************************************************************/

inline s64 jkm5506_device::apply_filters(jkm5506_voice *voice, s64 &sample, int n)
{
	/* pole 1 is always low-pass using K1 */
	sample = ((s32)(voice->k1 >> 2) * (sample - voice->o1n1[n]) / 16384) + voice->o1n1[n];
	voice->o1n1[n] = sample;

	/* pole 2 is always low-pass using K1 */
	sample = ((s32)(voice->k1 >> 2) * (sample - voice->o2n1[n]) / 16384) + voice->o2n1[n];
	voice->o2n2[n] = voice->o2n1[n];
	voice->o2n1[n] = sample;

	/* remaining poles depend on the current filter setting */
	switch (voice->control & CONTROL_LPMASK)
	{
		case 0:
			/* pole 3 is high-pass using K2 */
			sample = sample - voice->o2n2[n] + ((s32)(voice->k2 >> 2) * voice->o3n1[n]) / 32768 + voice->o3n1[n] / 2;
			voice->o3n2[n] = voice->o3n1[n];
			voice->o3n1[n] = sample;

			/* pole 4 is high-pass using K2 */
			sample = sample - voice->o3n2[n] + ((s32)(voice->k2 >> 2) * voice->o4n1[n]) / 32768 + voice->o4n1[n] / 2;
			voice->o4n1[n] = sample;
			break;

		case CONTROL_LP3:
			/* pole 3 is low-pass using K1 */
			sample = ((s32)(voice->k1 >> 2) * (sample - voice->o3n1[n]) / 16384) + voice->o3n1[n];
			voice->o3n2[n] = voice->o3n1[n];
			voice->o3n1[n] = sample;

			/* pole 4 is high-pass using K2 */
			sample = sample - voice->o3n2[n] + ((s32)(voice->k2 >> 2) * voice->o4n1[n]) / 32768 + voice->o4n1[n] / 2;
			voice->o4n1[n] = sample;
			break;

		case CONTROL_LP4:
			/* pole 3 is low-pass using K2 */
			sample = ((s32)(voice->k2 >> 2) * (sample - voice->o3n1[n]) / 16384) + voice->o3n1[n];
			voice->o3n2[n] = voice->o3n1[n];
			voice->o3n1[n] = sample;

			/* pole 4 is low-pass using K2 */
			sample = ((s32)(voice->k2 >> 2) * (sample - voice->o4n1[n]) / 16384) + voice->o4n1[n];
			voice->o4n1[n] = sample;
			break;

		case CONTROL_LP4 | CONTROL_LP3:
			/* pole 3 is low-pass using K1 */
			sample = ((s32)(voice->k1 >> 2) * (sample - voice->o3n1[n]) / 16384) + voice->o3n1[n];
			voice->o3n2[n] = voice->o3n1[n];
			voice->o3n1[n] = sample;

			/* pole 4 is low-pass using K2 */
			sample = ((s32)(voice->k2 >> 2) * (sample - voice->o4n1[n]) / 16384) + voice->o4n1[n];
			voice->o4n1[n] = sample;
			break;
	}
}



/**********************************************************************************************

     update_envelopes -- update the envelopes

***********************************************************************************************/

inline void jkm5506_device::update_envelopes(jkm5506_voice *voice, int samples)
{
	const int count = (samples > 1 && samples > voice->ecount) ? voice->ecount : samples;

	/* decrement the envelope counter */
	voice->ecount -= count;

	/* ramp left volume */
	if (voice->lvramp)
	{
		voice->lvol += (s8)voice->lvramp * count;
		if ((s32)voice->lvol < 0) voice->lvol = 0;
		else if (voice->lvol > 0xffff) voice->lvol = 0xffff;
	}

	/* ramp right volume */
	if (voice->rvramp)
	{
		voice->rvol += (s8)voice->rvramp * count;
		if ((s32)voice->rvol < 0) voice->rvol = 0;
		else if (voice->rvol > 0xffff) voice->rvol = 0xffff;
	}

	/* ramp k1 filter constant */
	if (voice->k1ramp && ((s32)voice->k1ramp >= 0 || !(voice->filtcount & 7)))
	{
		voice->k1 += (s8)voice->k1ramp * count;
		if ((s32)voice->k1 < 0) voice->k1 = 0;
		else if (voice->k1 > 0xffff) voice->k1 = 0xffff;
	}

	/* ramp k2 filter constant */
	if (voice->k2ramp && ((s32)voice->k2ramp >= 0 || !(voice->filtcount & 7)))
	{
		voice->k2 += (s8)voice->k2ramp * count;
		if ((s32)voice->k2 < 0) voice->k2 = 0;
		else if (voice->k2 > 0xffff) voice->k2 = 0xffff;
	}

	/* update the filter constant counter */
	voice->filtcount += count;

}



/**********************************************************************************************

     check_for_end_forward
     check_for_end_reverse -- check for loop end and loop appropriately

***********************************************************************************************/

#define check_for_end_forward(voice, accum)                                         \
do                                                                                  \
{                                                                                   \
	/* are we past the end? */                                                      \
	if (accum > voice->end && !(voice->control & CONTROL_LEI))                      \
	{                                                                               \
		/* generate interrupt if required */                                        \
		if (voice->control&CONTROL_IRQE)                                            \
			voice->control |= CONTROL_IRQ;                                          \
																					\
		/* handle the different types of looping */                                 \
		switch (voice->control & CONTROL_LOOPMASK)                                  \
		{                                                                           \
			/* non-looping */                                                       \
			case 0:                                                                 \
				voice->control |= CONTROL_STOP0;                                    \
				goto alldone;                                                       \
																					\
			/* uni-directional looping */                                           \
			case CONTROL_LPE:                                                       \
				accum = (voice->start + (accum - voice->end)) & voice->accum_mask;  \
				break;                                                              \
																					\
			/* trans-wave looping */                                                \
			case CONTROL_BLE:                                                       \
				accum = (voice->start + (accum - voice->end)) & voice->accum_mask;  \
				voice->control = (voice->control & ~CONTROL_LOOPMASK) | CONTROL_LEI;\
				break;                                                              \
																					\
			/* bi-directional looping */                                            \
			case CONTROL_LPE | CONTROL_BLE:                                         \
				accum = (voice->end - (accum - voice->end)) & voice->accum_mask;    \
				voice->control ^= CONTROL_DIR;                                      \
				goto reverse;                                                       \
		}                                                                           \
	}                                                                               \
} while (0)


#define check_for_end_reverse(voice, accum)                                         \
do                                                                                  \
{                                                                                   \
	/* are we past the end? */                                                      \
	if (accum < voice->start && !(voice->control & CONTROL_LEI))                    \
	{                                                                               \
		/* generate interrupt if required */                                        \
		if (voice->control&CONTROL_IRQE)                                            \
			voice->control |= CONTROL_IRQ;                                          \
																					\
		/* handle the different types of looping */                                 \
		switch (voice->control & CONTROL_LOOPMASK)                                  \
		{                                                                           \
			/* non-looping */                                                       \
			case 0:                                                                 \
				voice->control |= CONTROL_STOP0;                                    \
				goto alldone;                                                       \
																					\
			/* uni-directional looping */                                           \
			case CONTROL_LPE:                                                       \
				accum = (voice->end - (voice->start - accum)) & voice->accum_mask;  \
				break;                                                              \
																					\
			/* trans-wave looping */                                                \
			case CONTROL_BLE:                                                       \
				accum = (voice->end - (voice->start - accum)) & voice->accum_mask;  \
				voice->control = (voice->control & ~CONTROL_LOOPMASK) | CONTROL_LEI;\
				break;                                                              \
																					\
			/* bi-directional looping */                                            \
			case CONTROL_LPE | CONTROL_BLE:                                         \
				accum = (voice->start + (voice->start - accum)) & voice->accum_mask;\
				voice->control ^= CONTROL_DIR;                                      \
				goto reverse;                                                       \
		}                                                                           \
	}                                                                               \
} while (0)



/**********************************************************************************************

     generate_pcm -- general PCM decoding routine

***********************************************************************************************/

void jkm5506_device::generate_pcm(jkm5506_voice *voice, s32 *lbuffer, s32 *rbuffer, int samples)
{
	u64 freqcount = voice->freqcount;
	u64 accum = voice->accum & voice->accum_mask;
	s32 lvol = voice->lvol;
	s32 rvol = voice->rvol;

	/* outer loop, in case we switch directions */
	while (samples > 0 && !(voice->control & CONTROL_STOPMASK))
	{
reverse:
		/* two cases: first case is forward direction */
		if (!(voice->control & CONTROL_DIR))
		{
			/* loop while we still have samples to generate */
			while (samples--)
			{
				u32 dat1mask, dat1ls;
				u32 dat2mask, dat2ls;
				bool dat1_isulaw = false, dat2_isulaw = false;
				u32 dat1 = read_dword((accum >> 16) << 2);
				u32 dat2 = read_dword((((accum + (1 << 16)) & voice->accum_mask) >> 16) << 2);
				const u32 dat1shift = sample_endianswap(voice, dat1, dat1mask, dat1ls, dat1_isulaw);
				const u32 dat2shift = sample_endianswap(voice, dat2, dat2mask, dat2ls, dat2_isulaw);
				/* fetch two samples */
				s64 val1 = dat1 & dat1mask;
				s64 val2 = dat2 & dat2mask;
				s64 val3 = (dat1 >> dat1shift) & dat1mask;
				s64 val4 = (dat2 >> dat2shift) & dat2mask;

				/* decompress u-law */
				if (dat1_isulaw)
				{
					val1 = m_ulaw_lookup[val1 & 0xff];
					val3 = m_ulaw_lookup[val3 & 0xff];
				}
				else
				{
					val1 = s16(val1 << dat1ls);
					val3 = s16(val3 << dat1ls);
				}
				if (dat2_isulaw)
				{
					val2 = m_ulaw_lookup[val2 & 0xff];
					val4 = m_ulaw_lookup[val4 & 0xff];
				}
				else
				{
					val2 = s16(val2 << dat2ls);
					val4 = s16(val4 << dat2ls);
				}

				/* interpolate */
				interpolate(val1, val2, accum);
				interpolate(val3, val4, accum);
				accum = (accum + freqcount) & voice->accum_mask;

				/* apply filters */
				apply_filters(voice, val1, 0);
				apply_filters(voice, val3, 1);

				/* update filters/volumes */
				if (voice->ecount != 0)
				{
					update_envelopes(voice, 1);
					lvol = voice->lvol;
					rvol = voice->rvol;
				}

				if (voice->control & CONTROL_INVL)
				{
					lvol = -lvol;
				}
				if (voice->control & CONTROL_INVR)
				{
					rvol = -rvol;
				}

				/* apply volumes and add */
				*lbuffer++ += (val1 * lvol) >> 16;
				*rbuffer++ += (val3 * rvol) >> 16;

				/* check for loop end */
				check_for_end_forward(voice, accum);
			}
		}

		/* two cases: second case is backward direction */
		else
		{
			/* loop while we still have samples to generate */
			while (samples--)
			{
				u32 dat1mask, dat1ls;
				u32 dat2mask, dat2ls;
				bool dat1_isulaw = false, dat2_isulaw = false;
				u32 dat1 = read_dword((accum >> 16) << 2);
				u32 dat2 = read_dword((((accum + (1 << 16)) & voice->accum_mask) >> 16) << 2);
				const u32 dat1shift = sample_endianswap(voice, dat1, dat1mask, dat1ls, dat1_isulaw);
				const u32 dat2shift = sample_endianswap(voice, dat2, dat2mask, dat2ls, dat2_isulaw);
				/* fetch two samples */
				s64 val1 = dat1 & dat1mask;
				s64 val2 = dat2 & dat2mask;
				s64 val3 = (dat1 >> dat1shift) & dat1mask;
				s64 val4 = (dat2 >> dat2shift) & dat2mask;

				/* decompress u-law */
				if (dat1_isulaw)
				{
					val1 = m_ulaw_lookup[val1 & 0xff];
					val3 = m_ulaw_lookup[val3 & 0xff];
				}
				else
				{
					val1 = s16(val1 << dat1ls);
					val3 = s16(val3 << dat1ls);
				}
				if (dat2_isulaw)
				{
					val2 = m_ulaw_lookup[val2 & 0xff];
					val4 = m_ulaw_lookup[val4 & 0xff];
				}
				else
				{
					val2 = s16(val2 << dat2ls);
					val4 = s16(val4 << dat2ls);
				}

				/* interpolate */
				interpolate(val1, val2, accum);
				interpolate(val3, val4, accum);
				accum = (accum - freqcount) & voice->accum_mask;

				/* apply filters */
				apply_filters(voice, val1, 0);
				apply_filters(voice, val3, 1);

				/* update filters/volumes */
				if (voice->ecount != 0)
				{
					update_envelopes(voice, 1);
					lvol = voice->lvol;
					rvol = voice->rvol;
				}

				if (voice->control & CONTROL_INVL)
				{
					lvol = -lvol;
				}
				if (voice->control & CONTROL_INVR)
				{
					rvol = -rvol;
				}

				/* apply volumes and add */
				*lbuffer++ += (val1 * lvol) >> 16;
				*rbuffer++ += (val3 * rvol) >> 16;

				/* check for loop end */
				check_for_end_reverse(voice, accum);
			}
		}
	}

	/* if we stopped, process any additional envelope */
alldone:
	voice->accum = accum;
	if (samples > 0)
		update_envelopes(voice, samples);
}



/**********************************************************************************************

     generate_samples -- tell each voice to generate samples

***********************************************************************************************/

void jkm5506_device::generate_samples(s32 **outputs, int offset, int samples)
{
	int v;

	/* skip if nothing to do */
	if (!samples)
		return;

	/* clear out the accumulators */
	for (int i = 0; i < m_channels << 1; i++)
	{
		memset(outputs[i] + offset, 0, sizeof(s32) * samples);
	}

	/* loop over voices */
	for (v = 0; v <= m_active_voices; v++)
	{
		jkm5506_voice *voice = &m_voice[v];

		/* special case: if end == start, stop the voice */
		if (voice->start == voice->end)
			voice->control |= CONTROL_STOP0;

		int voice_channel = (voice->control & CONTROL_CAMASK) >> 10;
		int channel = voice_channel % m_channels;
		int l = channel << 1;
		int r = l + 1;
		s32 *left = outputs[l] + offset;
		s32 *right = outputs[r] + offset;

		/* generate from the appropriate source */
		generate_pcm(voice, left, right, samples);

		/* does this voice have it's IRQ bit raised? */
		if (voice->control&CONTROL_IRQ)
		{
			LOG("jkm5506: IRQ raised on voice %d!!\n",v);

			/* only update voice vector if existing IRQ is acked by host */
			if (m_irqv&0x80)
			{
				/* latch voice number into vector, and set high bit low */
				m_irqv=v&0x7f;

				/* take down IRQ bit on voice */
				voice->control&=~CONTROL_IRQ;

				/* inform host of irq */
				update_irq_state();
			}
		}
	}
}



/**********************************************************************************************

     reg_write -- handle a write to the selected JKM5506 register

***********************************************************************************************/

inline void jkm5506_device::reg_write_low(jkm5506_voice *voice, offs_t offset, u64 data)
{
	switch (offset)
	{
		case 0x00/8:    /* CR */
			voice->control = data & 0xffffff;
			LOG("voice %d, control=%08x\n", voice->index, voice->control);
			break;

		case 0x08/8:    /* FC */
			voice->freqcount = data & 0xffffff;
			LOG("voice %d, freq count=%08x\n", voice->index, voice->freqcount);
			break;

		case 0x10/8:    /* LVOL */
			voice->lvol = data & 0xffff;
			LOG("voice %d, left vol=%04x\n", voice->index, voice->lvol);
			break;

		case 0x18/8:    /* LVRAMP */
			voice->lvramp = (data & 0xff00) >> 8;
			LOG("voice %d, left vol ramp=%04x\n", voice->index, voice->lvramp);
			break;

		case 0x20/8:    /* RVOL */
			voice->rvol = data & 0xffff;
			LOG("voice %d, right vol=%04x\n", voice->index, voice->rvol);
			break;

		case 0x28/8:    /* RVRAMP */
			voice->rvramp = (data & 0xff00) >> 8;
			LOG("voice %d, right vol ramp=%04x\n", voice->index, voice->rvramp);
			break;

		case 0x30/8:    /* ECOUNT */
			voice->ecount = data & 0x1ff;
			voice->filtcount = 0;
			LOG("voice %d, envelope count=%04x\n", voice->index, voice->ecount);
			break;

		case 0x38/8:    /* K2 */
			voice->k2 = data & 0xffff;
			LOG("voice %d, K2=%04x\n", voice->index, voice->k2);
			break;

		case 0x40/8:    /* K2RAMP */
			voice->k2ramp = ((data & 0xff00) >> 8) | ((data & 0x0001) << 31);
			LOG("voice %d, K2 ramp=%04x\n", voice->index, voice->k2ramp);
			break;

		case 0x48/8:    /* K1 */
			voice->k1 = data & 0xffff;
			LOG("voice %d, K1=%04x\n", voice->index, voice->k1);
			break;

		case 0x50/8:    /* K1RAMP */
			voice->k1ramp = ((data & 0xff00) >> 8) | ((data & 0x0001) << 31);
			LOG("voice %d, K1 ramp=%04x\n", voice->index, voice->k1ramp);
			break;

		case 0x58/8:    /* ACTV */
		{
			m_active_voices = data & 0x3f;
			m_sample_rate = m_master_clock / (16 * (m_active_voices + 1));
			m_stream->set_sample_rate(m_sample_rate);
			if (!m_sample_rate_changed_cb.isnull())
				m_sample_rate_changed_cb(m_sample_rate);

			LOG("active voices=%d, sample_rate=%d\n", m_active_voices, m_sample_rate);
			break;
		}

		case 0x60/8:    /* MODE */
			m_mode = data & 0x1f;
			break;

		case 0x68/8:    /* PAR - read only */
		case 0x70/8:    /* IRQV - read only */
			break;

		case 0x78/8:    /* PAGE */
			m_voice_bank = data & 0x80;
			m_current_page = data & 0x7f;
			break;
	}
}

inline void jkm5506_device::reg_write_high(jkm5506_voice *voice, offs_t offset, u64 data)
{
	switch (offset)
	{
		case 0x00/8:    /* CR */
			voice->control = data & 0xffffff;
			LOG("voice %d, control=%08x\n", voice->index, voice->control);
			break;

		case 0x08/8:    /* START */
			voice->start = data & 0xffffffffffff0000;
			LOG("voice %d, loop start=%16x\n", voice->index, voice->start);
			break;

		case 0x10/8:    /* END */
			voice->end = data & 0xffffffffffffff00;
			LOG("voice %d, loop end=%16x\n", voice->index, voice->end);
			break;

		case 0x18/8:    /* ACCUM */
			voice->accum = data;
			LOG("voice %d, accum=%16x\n", voice->index, voice->accum);
			break;

		case 0x20/8:    /* O4(n-1) */
			for (int i = 0; i < 2; i++)
			{
				voice->o4n1[i] = (s32)(data << 14) >> 14;
				data >>= 32;
				LOG("voice %d, O4(n-1)=%05x\n", voice->index, voice->o4n1[i] & 0x3ffff);
			}
			break;

		case 0x28/8:    /* O3(n-1) */
			for (int i = 0; i < 2; i++)
			{
				voice->o3n1[i] = (s32)(data << 14) >> 14;
				data >>= 32;
				LOG("voice %d, O3(n-1)=%05x\n", voice->index, voice->o3n1[i] & 0x3ffff);
			}
			break;

		case 0x30/8:    /* O3(n-2) */
			for (int i = 0; i < 2; i++)
			{
				voice->o3n2[i] = (s32)(data << 14) >> 14;
				data >>= 32;
				LOG("voice %d, O3(n-2)=%05x\n", voice->index, voice->o3n2[i] & 0x3ffff);
			}
			break;

		case 0x38/8:    /* O2(n-1) */
			for (int i = 0; i < 2; i++)
			{
				voice->o2n1[i] = (s32)(data << 14) >> 14;
				data >>= 32;
				LOG("voice %d, O2(n-1)=%05x\n", voice->index, voice->o2n1[i] & 0x3ffff);
			}
			break;

		case 0x40/8:    /* O2(n-2) */
			for (int i = 0; i < 2; i++)
			{
				voice->o2n2[i] = (s32)(data << 14) >> 14;
				data >>= 32;
				LOG("voice %d, O2(n-2)=%05x\n", voice->index, voice->o2n2[i] & 0x3ffff);
			}
			break;

		case 0x48/8:    /* O1(n-1) */
			for (int i = 0; i < 2; i++)
			{
				voice->o1n1[i] = (s32)(data << 14) >> 14;
				data >>= 32;
				LOG("voice %d, O1(n-1)=%05x\n", voice->index, voice->o1n1[i] & 0x3ffff);
			}
			break;

		case 0x50/8:    /* W_ST */
			m_wst = data & 0x7f;
			break;

		case 0x58/8:    /* W_END */
			m_wend = data & 0x7f;
			break;

		case 0x60/8:    /* LR_END */
			m_lrend = data & 0x7f;
			break;

		case 0x68/8:    /* PAR - read only */
		case 0x70/8:    /* IRQV - read only */
			break;

		case 0x78/8:    /* PAGE */
			m_voice_bank = data & 0x80;
			m_current_page = data & 0x7f;
			break;
	}
}

inline void jkm5506_device::reg_write_test(jkm5506_voice *voice, offs_t offset, u64 data)
{
	switch (offset)
	{
		case 0x00/8:    /* CHANNEL 0 LEFT */
			LOG("Channel 0 left test write %08x\n", data);
			break;

		case 0x08/8:    /* CHANNEL 0 RIGHT */
			LOG("Channel 0 right test write %08x\n", data);
			break;

		case 0x10/8:    /* CHANNEL 1 LEFT */
			LOG("Channel 1 left test write %08x\n", data);
			break;

		case 0x18/8:    /* CHANNEL 1 RIGHT */
			LOG("Channel 1 right test write %08x\n", data);
			break;

		case 0x20/8:    /* CHANNEL 2 LEFT */
			LOG("Channel 2 left test write %08x\n", data);
			break;

		case 0x28/8:    /* CHANNEL 2 RIGHT */
			LOG("Channel 2 right test write %08x\n", data);
			break;

		case 0x30/8:    /* CHANNEL 3 LEFT */
			LOG("Channel 3 left test write %08x\n", data);
			break;

		case 0x38/8:    /* CHANNEL 3 RIGHT */
			LOG("Channel 3 right test write %08x\n", data);
			break;

		case 0x40/8:    /* CHANNEL 4 LEFT */
			LOG("Channel 4 left test write %08x\n", data);
			break;

		case 0x48/8:    /* CHANNEL 4 RIGHT */
			LOG("Channel 4 right test write %08x\n", data);
			break;

		case 0x50/8:    /* CHANNEL 5 LEFT */
			LOG("Channel 5 left test write %08x\n", data);
			break;

		case 0x58/8:    /* CHANNEL 6 RIGHT */
			LOG("Channel 5 right test write %08x\n", data);
			break;

		case 0x60/8:    /* EMPTY */
			LOG("Test write EMPTY %08x\n", data);
			break;

		case 0x68/8:    /* PAR - read only */
		case 0x70/8:    /* IRQV - read only */
			break;

		case 0x78/8:    /* PAGE */
			m_voice_bank = data & 0x80;
			m_current_page = data & 0x7f;
			break;
	}
}

void jkm5506_device::reg_w(offs_t offset, u8 data)
{
	jkm5506_voice *voice = &m_voice[(m_voice_bank >> 2) | (m_current_page & 0x1f)];

	/* switch off the page and register */
	if (m_current_page < 0x20)
		reg_write_low(voice, offset, m_write_latch);
	else if (m_current_page < 0x40)
		reg_write_high(voice, offset, m_write_latch);
	else
		reg_write_test(voice, offset, m_write_latch);

	/* clear the write latch when done */
	m_write_latch = 0;
}



/**********************************************************************************************

     reg_read -- read from the specified JKM5506 register

***********************************************************************************************/

inline u64 jkm5506_device::reg_read_low(jkm5506_voice *voice, offs_t offset)
{
	u64 result = 0;

	switch (offset)
	{
		case 0x00/8:    /* CR */
			result = voice->control;
			break;

		case 0x08/8:    /* FC */
			result = voice->freqcount;
			break;

		case 0x10/8:    /* LVOL */
			result = voice->lvol;
			break;

		case 0x18/8:    /* LVRAMP */
			result = voice->lvramp << 8;
			break;

		case 0x20/8:    /* RVOL */
			result = voice->rvol;
			break;

		case 0x28/8:    /* RVRAMP */
			result = voice->rvramp << 8;
			break;

		case 0x30/8:    /* ECOUNT */
			result = voice->ecount;
			break;

		case 0x38/8:    /* K2 */
			result = voice->k2;
			break;

		case 0x40/8:    /* K2RAMP */
			result = (voice->k2ramp << 8) | (voice->k2ramp >> 31);
			break;

		case 0x48/8:    /* K1 */
			result = voice->k1;
			break;

		case 0x50/8:    /* K1RAMP */
			result = (voice->k1ramp << 8) | (voice->k1ramp >> 31);
			break;

		case 0x58/8:    /* ACTV */
			result = m_active_voices;
			break;

		case 0x60/8:    /* MODE */
			result = m_mode;
			break;

		case 0x68/8:    /* PAR */
			if (!m_read_port_cb.isnull())
				result = m_read_port_cb(0);
			break;

		case 0x70/8:    /* IRQV */
			result = m_irqv;
			update_internal_irq_state();
			break;

		case 0x78/8:    /* PAGE */
			result = m_voice_bank | m_current_page;
			break;
	}
	return result;
}


inline u64 jkm5506_device::reg_read_high(jkm5506_voice *voice, offs_t offset)
{
	u64 result = 0;

	switch (offset)
	{
		case 0x00/8:    /* CR */
			result = voice->control;
			break;

		case 0x08/8:    /* START */
			result = voice->start;
			break;

		case 0x10/8:    /* END */
			result = voice->end;
			break;

		case 0x18/8:    /* ACCUM */
			result = voice->accum;
			break;

		case 0x20/8:    /* O4(n-1) */
			for (int i = 1; i >= 0; i--)
			{
				result |= voice->o4n1[i] & 0x3ffff;
				result <<= 32;
			}
			break;

		case 0x28/8:    /* O3(n-1) */
			for (int i = 1; i >= 0; i--)
			{
				result |= voice->o3n1[i] & 0x3ffff;
				result <<= 32;
			}
			break;

		case 0x30/8:    /* O3(n-2) */
			for (int i = 1; i >= 0; i--)
			{
				result |= voice->o3n2[i] & 0x3ffff;
				result <<= 32;
			}
			break;

		case 0x38/8:    /* O2(n-1) */
			for (int i = 1; i >= 0; i--)
			{
				result |=  voice->o2n1[i] & 0x3ffff;
				result <<= 32;
			}
			break;

		case 0x40/8:    /* O2(n-2) */
			for (int i = 1; i >= 0; i--)
			{
				result |= voice->o2n2[i] & 0x3ffff;
				result <<= 32;
			}
			break;

		case 0x48/8:    /* O1(n-1) */
			for (int i = 1; i >= 0; i--)
			{
				result |= voice->o1n1[i] & 0x3ffff;
				result <<= 32;
			}
			break;

		case 0x50/8:    /* W_ST */
			result = m_wst;
			break;

		case 0x58/8:    /* W_END */
			result = m_wend;
			break;

		case 0x60/8:    /* LR_END */
			result = m_lrend;
			break;

		case 0x68/8:    /* PAR */
			if (!m_read_port_cb.isnull())
				result = m_read_port_cb(0);
			break;

		case 0x70/8:    /* IRQV */
			result = m_irqv;
			update_internal_irq_state();
			break;

		case 0x78/8:    /* PAGE */
			result = m_voice_bank | m_current_page;
			break;
	}
	return result;
}
inline u64 jkm5506_device::reg_read_test(jkm5506_voice *voice, offs_t offset)
{
	u64 result = 0;

	switch (offset)
	{
		case 0x68/8:    /* PAR */
			if (!m_read_port_cb.isnull())
				result = m_read_port_cb(0);
			break;

		case 0x70/8:    /* IRQV */
			result = m_irqv;
			break;

		case 0x78/8:    /* PAGE */
			result = m_voice_bank | m_current_page;
			break;
	}
	return result;
}

void jkm5506_device::reg_r(offs_t offset, u8 data)
{
	jkm5506_voice *voice = &m_voice[(m_voice_bank >> 2) | (m_current_page & 0x1f)];

	/* force an update */
	m_stream->update();

	/* switch off the page and register */
	if (m_current_page < 0x20)
		m_read_latch = reg_read_low(voice, offset);
	else if (m_current_page < 0x40)
		m_read_latch = reg_read_high(voice, offset);
	else
		m_read_latch = reg_read_test(voice, offset);

	LOG("%16x\n", m_read_latch);
}


//-------------------------------------------------
//  sound_stream_update - handle a stream update
//-------------------------------------------------

void jkm5506_device::sound_stream_update(sound_stream &stream, stream_sample_t **inputs, stream_sample_t **outputs, int samples)
{
#if JKM5506_MAKE_WAVS
	/* start the logging once we have a sample rate */
	if (m_sample_rate)
	{
		if (!m_wavraw)
			m_wavraw = wav_open("raw.wav", m_sample_rate, 2);
	}
#endif

	/* loop until all samples are output */
	int offset = 0;
	while (samples)
	{
		int length = (samples > MAX_SAMPLE_CHUNK) ? MAX_SAMPLE_CHUNK : samples;

		generate_samples(outputs, offset, length);

#if JKM5506_MAKE_WAVS
		/* log the raw data */
		if (m_wavraw) {
			/* determine left/right source data */
			s32 *lsrc = m_scratch, *rsrc = m_scratch + length;
			int channel;
			memset(lsrc, 0, sizeof(s32) * length * 2);
			/* loop over the output channels */
			for (channel = 0; channel < m_channels; channel++) {
				s32 *l = outputs[(channel << 1)] + offset;
				s32 *r = outputs[(channel << 1) + 1] + offset;
				/* add the current channel's samples to the WAV data */
				for (samp = 0; samp < length; samp++) {
					lsrc[samp] += l[samp];
					rsrc[samp] += r[samp];
				}
			}
			wav_add_data_32lr(m_wavraw, lsrc, rsrc, length, 4);
		}
#endif

		/* account for these samples */
		offset += length;
		samples -= length;
	}
}
