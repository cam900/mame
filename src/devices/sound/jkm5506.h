// license:BSD-3-Clause
// copyright-holders:Aaron Giles
/**********************************************************************************************

	MATRIX JKM5506 OTTO4
	ES5506 with 64 voices, 50 bit sample address bit (1PB samples), Stereo / Invert output
	Operation clock ~100MHz, output rate clock / (16 * active voice)
	65.536KHz at 67.108864MHz, 64 voice
	frequency : output rate / 65536

***********************************************************************************************/

#ifndef MAME_SOUND_JKM5506_H
#define MAME_SOUND_JKM5506_H

#pragma once

#define JKM5506_MAKE_WAVS 0

class jkm5506_device : public device_t, public device_sound_interface, public device_rom_interface
{
public:
	jkm5506_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock);
	~jkm5506_device() {}

	void reg_r(offs_t offset, u8 data);
	void reg_w(offs_t offset, u8 data);
	void latch_w(offs_t offset, u64 data, u64 mem_mask) { COMBINE_DATA(&m_write_latch); }
	u64 latch_r(offs_t offset, u64 mem_mask) { return m_read_latch & mem_mask; }
	void voice_bank_w(int voice, int bank);

	void set_channels(int channels) { m_channels = channels; }

	auto irq_cb() { return m_irq_cb.bind(); }
	auto read_port_cb() { return m_read_port_cb.bind(); }
	auto sample_rate_changed() { return m_sample_rate_changed_cb.bind(); }

protected:
	// device-level overrides
	virtual void device_start() override;
	virtual void device_clock_changed() override;
	virtual void device_stop() override;
	virtual void device_reset() override;

	// sound stream update overrides
	virtual void sound_stream_update(sound_stream &stream, stream_sample_t **inputs, stream_sample_t **outputs, int samples) override;

	// device_rom_interface overrides
	virtual void rom_bank_updated() override;

private:
	// struct describing a single playing voice
	struct jkm5506_voice
	{
		jkm5506_voice() { }

		// external state
		u32      control   = 0;          // control register
		u64      freqcount = 0;          // frequency count register
		u64      start     = 0;          // start register
		u32      lvol      = 0;          // left volume register
		u64      end       = 0;          // end register
		u32      lvramp    = 0;          // left volume ramp register
		u64      accum     = 0;          // accumulator register
		u32      rvol      = 0;          // right volume register
		u32      rvramp    = 0;          // right volume ramp register
		u32      ecount    = 0;          // envelope count register
		u32      k2        = 0;          // k2 register
		u32      k2ramp    = 0;          // k2 ramp register
		u32      k1        = 0;          // k1 register
		u32      k1ramp    = 0;          // k1 ramp register
		s32      o4n1[2]   = {0};          // filter storage O4(n-1)
		s32      o3n1[2]   = {0};          // filter storage O3(n-1)
		s32      o3n2[2]   = {0};          // filter storage O3(n-2)
		s32      o2n1[2]   = {0};          // filter storage O2(n-1)
		s32      o2n2[2]   = {0};          // filter storage O2(n-2)
		s32      o1n1[2]   = {0};          // filter storage O1(n-1)

		// internal state
		u8       index      = 0;         // index of this voice
		u8       filtcount  = 0;         // filter count
		u64      accum_mask = 0;
	};

	void update_irq_state();
	void update_internal_irq_state();
	void compute_tables();

	void generate_samples(s32 **outputs, int offset, int samples);
	void generate_pcm(jkm5506_voice *voice, s32 *lbuffer, s32 *rbuffer, int samples);
	inline u32 sample_endianswap(jkm5506_voice *voice, u32 &data, u32 &mask, u32 &shift, bool &is_ulaw);
	inline s64 apply_filters(jkm5506_voice *voice, s64 &sample, int n);
	inline void update_envelopes(jkm5506_voice *voice, int samples);

	// internal state
	sound_stream *m_stream;               /* which stream are we using */
	int      m_sample_rate;            /* current sample rate */
	u64      m_write_latch;            /* currently accumulated data for write */
	u64      m_read_latch;             /* currently accumulated data for read */
	u32      m_master_clock;           /* master clock frequency */

	u8       m_voice_bank;
	u8       m_current_page;           /* current register page */
	u8       m_active_voices;          /* number of active voices */
	u8       m_mode;                   /* MODE register */
	u8       m_wst;                    /* W_ST register */
	u8       m_wend;                   /* W_END register */
	u8       m_lrend;                  /* LR_END register */
	u8       m_irqv;                   /* IRQV register */

	jkm5506_voice m_voice[64];             /* the 64 voices */

	std::unique_ptr<s32[]>     m_scratch;

	std::unique_ptr<s16[]>    m_ulaw_lookup;

#if JKM5506_MAKE_WAVS
	void *      m_wavraw;                 /* raw waveform */
#endif

	const char * m_region0;                       /* memory region where the sample ROM lives */
	const char * m_region1;                       /* memory region where the sample ROM lives */
	const char * m_region2;                       /* memory region where the sample ROM lives */
	const char * m_region3;                       /* memory region where the sample ROM lives */
	int m_channels;                               /* number of output channels: 1 .. 6 */
	devcb_write_line m_irq_cb;  /* irq callback */
	devcb_read16 m_read_port_cb;          /* input port read */
	devcb_write32 m_sample_rate_changed_cb;          /* callback for when sample rate is changed */

	inline void reg_write_low(jkm5506_voice *voice, offs_t offset, u64 data);
	inline void reg_write_high(jkm5506_voice *voice, offs_t offset, u64 data);
	inline void reg_write_test(jkm5506_voice *voice, offs_t offset, u64 data);
	inline u64 reg_read_low(jkm5506_voice *voice, offs_t offset);
	inline u64 reg_read_high(jkm5506_voice *voice, offs_t offset);
	inline u64 reg_read_test(jkm5506_voice *voice, offs_t offset);
};


DECLARE_DEVICE_TYPE(JKM5506, jkm5506_device)


#endif // MAME_SOUND_JKM5506_H
