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


#ifndef MAME_SOUND_JKM280B_H
#define MAME_SOUND_JKM280B_H

#pragma once


#define JKM280B_MAKE_WAVS 0

class jkm280b_device : public device_t, public device_sound_interface, public device_rom_interface
{
public:
	jkm280b_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	// configuration helpers
	auto irq_handler() { return m_irq_handler.bind(); }

	u8 read8(offs_t offset);
	void write8(offs_t offset, u8 data, u8 mem_mask);

	u16 read16(offs_t offset, u16 mem_mask);
	void write16(offs_t offset, u16 data, u16 mem_mask);

protected:
	// device-level overrides
	virtual void device_start() override;
	virtual void device_post_load() override;
	virtual void device_reset() override;
	virtual void device_timer(emu_timer &timer, device_timer_id id, int param, void *ptr) override;
	virtual void device_clock_changed() override;

	// sound stream update overrides
	virtual void sound_stream_update(sound_stream &stream, stream_sample_t **inputs, stream_sample_t **outputs, int samples) override;

	// device_rom_interface overrides
	virtual void rom_bank_updated() override;

private:
	/* struct describing a single playing ADPCM voice */
	struct JKM280BVoice
	{
		uint8_t playing;          /* 1 if we are actively playing */
		bool stereo;
		bool ended;             /* indicate voice has ended in case samples_left is 0 */

		uint8_t keyon;            /* 1 if the key is on */
		uint8_t looping;          /* 1 if looping is enabled */
		uint8_t mode;             /* current playback mode */
		uint32_t fnum;            /* frequency */

		uint64_t start;           /* start address, in nibbles */
		uint64_t stop;            /* stop address, in nibbles */
		uint64_t loop_start;      /* loop start address, in nibbles */
		uint64_t loop_end;        /* loop end address, in nibbles */
		uint64_t position;        /* current position, in nibbles */

		int32_t signal[2];        /* current ADPCM signal */
		int32_t step[2];          /* current ADPCM step */

		int32_t loop_signal[2];   /* signal at loop start */
		int32_t loop_step[2];     /* step at loop start */
		uint32_t loop_count;      /* number of loops so far */

		int32_t output_left;      /* output volume (left) */
		int32_t output_right;     /* output volume (right) */
		int32_t output_step;      /* step value for frequency conversion */
		int32_t output_pos;       /* current fractional position */
		int16_t last_lsample;     /* last sample output */
		int16_t last_rsample;     /* last sample output */
		int16_t curr_lsample;     /* current sample target */
		int16_t curr_rsample;     /* current sample target */
		uint8_t irq_schedule;     /* 1 if the IRQ state is updated by timer */

		emu_timer *timer;
	};

	void update_irq_state();
	void update_step(struct JKM280BVoice *voice);
	void update_irq_state_timer_common(int voicenum);
	int generate_adpcm(struct JKM280BVoice *voice, int16_t *lbuffer, int16_t *rbuffer, int samples);
	int generate_pcm8(struct JKM280BVoice *voice, int16_t *lbuffer, int16_t *rbuffer, int samples);
	int generate_pcm16(struct JKM280BVoice *voice, int16_t *lbuffer, int16_t *rbuffer, int samples);
	void write_to_register(u16 data, u16 mem_mask = ~0);
	u16 compute_status();

	// internal state
	struct JKM280BVoice m_voice[16];    /* the 16 voices */
	uint8_t m_current_register;         /* currently accessible register */
	uint16_t m_status_register;          /* current status register */
	uint16_t m_irq_state;                /* current IRQ state */
	uint16_t m_irq_mask;                 /* current IRQ mask */
	bool m_irq_enable;               /* current IRQ enable */
	bool m_keyon_enable;             /* key on enable */
	bool m_ext_mem_enable;           /* external memory enable */
	uint8_t m_ext_readlatch;            /* external memory prefetched data */
	uint32_t m_ext_mem_address_hi;
	uint32_t m_ext_mem_address;         /* where the CPU can read the ROM */

	devcb_write_line m_irq_handler;  /* IRQ callback */

	double m_master_clock;            /* master clock frequency */
	sound_stream *m_stream;           /* which stream are we using */
	std::unique_ptr<int16_t[]> m_scratch[2];
#if JKM280B_MAKE_WAVS
	void *m_wavresample;              /* resampled waveform */
#endif
};

DECLARE_DEVICE_TYPE(JKM280B, jkm280b_device)

#endif // MAME_SOUND_JKM280B_H
