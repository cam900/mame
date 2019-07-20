/*
	MATRIX 32GS SUPERFAST
	Custom programmed 10CX105YF780E5G
*/

#include "emu.h"
#include "screen.h"

#include <algorithm>

#define PIXEL_CLOCK     787320000

//sound/matrix32gs.h
class matrix32gs_sound_device : public device_t, public device_sound_interface, public device_memory_interface
{
public:
	matrix32gs_sound_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock);
	~matrix32gs_sound_device() { }

protected:
	// device-level overrides
	virtual void device_start() override;
	virtual void device_clock_changed() override;

	// sound stream update overrides
	virtual void sound_stream_update(sound_stream &stream, stream_sample_t **inputs, stream_sample_t **outputs, int samples) override;

	// device_memory_interface configuration
	virtual space_config_vector memory_space_config() const override;

	address_space_config m_data_config;
private:
	/*
		Channel registers

	00	k--- ---- ---- ---- ---- ---- ---- ---- Execute keyon
		-o-- ---- ---- ---- ---- ---- ---- ---- Keyon bit
		--ll ---- ---- ---- ---- ---- ---- ---- Loop mode (off, Linear, Reverse, Pingpong)
		---- i--- ---- ---- ---- ---- ---- ---- Interpolate enable / disable
		---- -f-- ---- ---- ---- ---- ---- ---- FM enable / disable
		---- --a- ---- ---- ---- ---- ---- ---- AM enable / disable
		---- ---f ---- ---- ---- ---- ---- ---- Filter enable / disable
		---- ---- s--- ---- ---- ---- ---- ---- Stereo sample
		---- ---- -f-- ---- ---- ---- ---- ---- Sample format (8bit / 16bit)
		---- ---- --e- ---- ---- ---- ---- ---- Swap stereo
		---- ---- ---s ---- ---- ---- ---- ---- Toggle Unsigned / Signed sample
		---- ---- ---- l--- ---- ---- ---- ---- Pitch LFO enable
		---- ---- ---- -l-- ---- ---- ---- ---- Volume LFO enable
		---- ---- ---- --e- ---- ---- ---- ---- Envelope enable
		---- ---- ---- ---p ---- ---- ---- ---- Pause
		---- ---- ---- ---- mmmm mmmm mmmm mmmm Master volume (16 bit unsigned)

	04	llll llll llll llll ---- ---- ---- ---- Left volume (16 bit signed)
		---- ---- ---- ---- rrrr rrrr rrrr rrrr Right volume (16 bit signed)

	08	ssss ssss ssss ssss ssss ssss ssss ssss Start

	0c	llll llll llll llll llll llll llll llll Loop

	10	eeee eeee eeee eeee eeee eeee eeee eeee Loopend

	14	eeee eeee eeee eeee eeee eeee eeee eeee End

	18	---- ---- ---- --ff ffff ffff ffff ffff Sample frequency

	1c	oooo pppp pppp pppp ---- ---- ---- ---- Sample pitch ((0x1000 | p) << (o ^ 8), 0 at 100% (8.20 fixed point))

	20	ffff ---- ---- ---- ---- ---- ---- ---- FM Scale
		---- f--- ---- ---- ---- ---- ---- ---- FM Stack write enable / disable
		---- -ii- ---- ---- ---- ---- ---- ---- Number of FM inputs
		---- ---d ---- ---- ---- ---- ---- ---- FM Stack write directly / volumed
		---- ---- wwww wwww ---- ---- ---- ---- FM Write stack offset
		---- ---- ---- ---- w--- ---- ---- ---- FM Write stack offset MSB
		---- ---- ---- ---- ---a ---- ---- ---- FM Input A stack offset MSB
		---- ---- ---- ---- ---- ---a ---- ---- FM Input B stack offset MSB
		---- ---- ---- ---- ---- ---- ---a ---- FM Input C stack offset MSB
		---- ---- ---- ---- ---- ---- ---- ---a FM Input D stack offset MSB

	24	aaaa aaaa ---- ---- ---- ---- ---- ---- FM Input A stack offset
		---- ---- aaaa aaaa ---- ---- ---- ---- FM Input B stack offset
		---- ---- ---- ---- aaaa aaaa ---- ---- FM Input C stack offset
		---- ---- ---- ---- ---- ---- aaaa aaaa FM Input D stack offset

	28	ffff ---- ---- ---- ---- ---- ---- ---- AM Scale
		---- f--- ---- ---- ---- ---- ---- ---- AM Stack write enable / disable
		---- -ii- ---- ---- ---- ---- ---- ---- Number of AM inputs
		---- ---d ---- ---- ---- ---- ---- ---- AM Stack write directly / volumed
		---- ---- wwww wwww ---- ---- ---- ---- AM Write stack offset
		---- ---- ---- ---- w--- ---- ---- ---- AM Write stack offset MSB
		---- ---- ---- ---- ---a ---- ---- ---- AM Input A stack offset MSB
		---- ---- ---- ---- ---- ---a ---- ---- AM Input B stack offset MSB
		---- ---- ---- ---- ---- ---- ---a ---- AM Input C stack offset MSB
		---- ---- ---- ---- ---- ---- ---- ---a AM Input D stack offset MSB

	2c	aaaa aaaa ---- ---- ---- ---- ---- ---- AM Input A stack offset
		---- ---- aaaa aaaa ---- ---- ---- ---- AM Input B stack offset
		---- ---- ---- ---- aaaa aaaa ---- ---- AM Input C stack offset
		---- ---- ---- ---- ---- ---- aaaa aaaa AM Input D stack offset

	30	vvvv vvvv vvvv vvvv vvvv vvvv vvvv vvvv Envelope volume initial (32 bit unsigned)

	34	dddd dddd dddd dddd dddd dddd dddd dddd Envelope delay rate

	38	vvvv vvvv vvvv vvvv vvvv vvvv vvvv vvvv Add envelope volume rate for every envelope delay steps

	3c	vvvv vvvv vvvv vvvv vvvv vvvv vvvv vvvv Add envelope volume add rate for every envelope delay steps

	40	dddd dddd dddd dddd dddd dddd dddd dddd Envelope attack rate

	44	vvvv vvvv vvvv vvvv vvvv vvvv vvvv vvvv Add envelope volume rate for every envelope attack steps

	48	vvvv vvvv vvvv vvvv vvvv vvvv vvvv vvvv Add envelope volume add rate for every envelope attack steps

	4c	dddd dddd dddd dddd dddd dddd dddd dddd Envelope hold rate

	50	vvvv vvvv vvvv vvvv vvvv vvvv vvvv vvvv Add envelope volume rate for every envelope hold steps

	54	vvvv vvvv vvvv vvvv vvvv vvvv vvvv vvvv Add envelope volume add rate for every envelope hold steps

	58	dddd dddd dddd dddd dddd dddd dddd dddd Envelope decay rate

	5c	vvvv vvvv vvvv vvvv vvvv vvvv vvvv vvvv Add envelope volume rate for every envelope decay steps

	60	vvvv vvvv vvvv vvvv vvvv vvvv vvvv vvvv Add envelope volume add rate for every envelope decay steps

	64	dddd dddd dddd dddd dddd dddd dddd dddd Envelope sustain rate

	68	vvvv vvvv vvvv vvvv vvvv vvvv vvvv vvvv Add envelope volume rate for every envelope sustain steps

	6c	vvvv vvvv vvvv vvvv vvvv vvvv vvvv vvvv Add envelope volume add rate for every envelope sustain steps

	70	dddd dddd dddd dddd dddd dddd dddd dddd Envelope release rate

	74	ssss ssss ssss ssss ssss ssss ssss ssss Subtract envelope volume for every envelope release steps

	78	---- ---- ---- ---- ---- ---- ---- i--- Volume LFO Invert initialize
		---- ---- ---- ---- ---- ---- ---- -p-- Volume LFO Pingpong toggle
		---- ---- ---- ---- ---- ---- ---- --ww Volume LFO wave

	7c	dddd dddd dddd dddd dddd dddd dddd dddd Volume LFO delay rate

	80	vvvv vvvv vvvv vvvv vvvv vvvv vvvv vvvv Volume LFO scale (32 bit signed)

	84	aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa Volume LFO attack rate

	88	ssss ssss ssss ssss ssss ssss ssss ssss Volume LFO step rate

	8c	vvvv vvvv vvvv vvvv vvvv vvvv vvvv vvvv Modify Volume LFO level for every Volume LFO steps

	90	vvvv vvvv vvvv vvvv vvvv vvvv vvvv vvvv Modify Volume LFO add level for every Volume LFO steps

	94	---- ---- ---- ---- ---- ---- ---- i--- Pitch LFO Invert initialize
		---- ---- ---- ---- ---- ---- ---- -p-- Pitch LFO Pingpong toggle
		---- ---- ---- ---- ---- ---- ---- --ww Pitch LFO wave

	98	dddd dddd dddd dddd dddd dddd dddd dddd Pitch LFO delay rate

	9c	vvvv vvvv vvvv vvvv vvvv vvvv vvvv vvvv Pitch LFO scale (32 bit signed)

	a0	aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa Pitch LFO attack rate

	a4	ssss ssss ssss ssss ssss ssss ssss ssss Pitch LFO step rate

	a8	vvvv vvvv vvvv vvvv vvvv vvvv vvvv vvvv Modify Pitch LFO level for every Pitch LFO steps

	ac	vvvv vvvv vvvv vvvv vvvv vvvv vvvv vvvv Modify Pitch LFO add level for every Pitch LFO steps

	b0	e--- ---- ---- ---- ---- ---- ---- ---- Filter A enable
		-s-- ---- ---- ---- ---- ---- ---- ---- Filter A direction (Low-pass, High-pass)
		--ff ffff ffff ffff ffff ffff ffff ffff Filter A

	b4	e--- ---- ---- ---- ---- ---- ---- ---- Filter B enable
		-s-- ---- ---- ---- ---- ---- ---- ---- Filter B direction (Low-pass, High-pass)
		--ff ffff ffff ffff ffff ffff ffff ffff Filter B

	b8	e--- ---- ---- ---- ---- ---- ---- ---- Filter C enable
		-s-- ---- ---- ---- ---- ---- ---- ---- Filter C direction (Low-pass, High-pass)
		--ff ffff ffff ffff ffff ffff ffff ffff Filter C

	bc	e--- ---- ---- ---- ---- ---- ---- ---- Filter D enable
		-s-- ---- ---- ---- ---- ---- ---- ---- Filter D direction (Low-pass, High-pass)
		--ff ffff ffff ffff ffff ffff ffff ffff Filter D

	c0	ssss ssss ssss ssss ssss ssss ssss ssss SubEG Rate

	c4	s--- ---- ---- ---- ---- ---- ---- ---- SubEG enable
		-s-- ---- ---- ---- ---- ---- ---- ---- SubEG invert
		--ss ss-- ---- ---- ---- ---- ---- ---- SubEG shape

	*/
	struct fm_am_t
	{
		fm_am_t()
		{
		}

		bool enable = false;
		bool write_enable = false;
		bool write_directly = false;
		u8 input_no = 0;
		u8 scale = 0;
		u16 inputoffs[4] = {0,0,0,0};
		u16 outputoffs = 0;
		void tick(u16 addr, s32 *buffer_l, s32 *buffer_r);
		void write(u16 addr, s32 *buffer_l, s32 *buffer_r, s32 &left, s32 &right);
		s64 output[2] = {0,0};
	};

	struct envelope_t
	{
		envelope_t()
		{
		}

		u8 status = 0;
		u32 curr_rate = 0;
		s32 curr_step = 0;
		s64 curr_vol = 0;
		u32 init_vol = 0;
		u32 delay_rate = 0;
		s32 delay_step = 0;
		s32 delay_inc = 0;
		u32 attack_rate = 0;
		s32 attack_step = 0;
		s32 attack_inc = 0;
		u32 hold_rate = 0;
		s32 hold_step = 0;
		s32 hold_inc = 0;
		u32 decay_rate = 0;
		s32 decay_step = 0;
		s32 decay_inc = 0;
		u32 sustain_rate = 0;
		s32 sustain_step = 0;
		s32 sustain_inc = 0;
		u32 release_rate = 0;
		u32 release_step = 0;
		void keyon();
		bool tick();
	};

	struct lfo_t
	{
		lfo_t()
		{
		}

		s32 scale = 0;
		u8 status = 0;
		bool invert = false;
		bool pingpong = false;
		bool sign = false;
		bool wait = false;
		u8 wave = 0;
		u32 curr_rate = 0;
		s32 curr_step = 0;
		s64 curr_vol = 0;
		u32 delay_remain = 0;
		u32 delay_rate = 0;
		u32 attack_remain = 0;
		u32 curr_attack_step = 0;
		u32 attack_rate = 0;
		s32 attack_step = 0;
		s32 attack_inc = 0;
		s64 attack_scale = 0;
		u32 loop_rate = 0;
		s32 loop_step = 0;
		s32 loop_inc = 0;
		void keyon();
		void tick();
		s64 output = 0;
	};

	struct filter_t
	{
		bool filter_a_en = false;
		bool filter_b_en = false;
		bool filter_c_en = false;
		bool filter_d_en = false;
		bool filter_a_dir = false;
		bool filter_b_dir = false;
		bool filter_c_dir = false;
		bool filter_d_dir = false;
		s32 filter_a = 0;
		s32 filter_b = 0;
		s32 filter_c = 0;
		s32 filter_d = 0;
		s32 history_l[4 * 2] = {0};
		s32 history_r[4 * 2] = {0};
		void tick(s32 &left, s32 &right);
	};

	struct subeg_t
	{
		bool enable = false;
		u8 invert = false;
		u8 invert_counter = false;
		u8 sign = false;
		u8 sign_counter = false;
		bool hold = false;
		u64 counter = 0;
		u64 rate = 0;
		u8 shape = 0;
		s64 curr_vol = 0;
		void keyon();
		void tick();
	};

	struct channel_t
	{
		channel_t()
		{
		}

		bool is_keyon = false;
		u32 reg = 0;
		u32 start = 0;
		u32 loop = 0;
		u32 loopend = 0;
		u32 end = 0;
		u32 freq = 0;
		u32 pos = 0;
		u32 frac = 0;
		u32 loopmode = 0;
		s16 lvol = 0;
		s16 rvol = 0;
		u16 mvol = 0;
		u32 pitch = 0;
		u32 reverse = 0;
		bool interpolate = false;
		s32 prev_sample[2] = {0,0};
		s32 curr_sample[2] = {0,0};
		fm_am_t fm;
		fm_am_t am;
		lfo_t volume_lfo;
		lfo_t pitch_lfo;
		envelope_t envelope;
		filter_t filter;
		void keyon();
		void keyoff(bool release);
		void tick();
		s64 output[2] = {0,0};
	};

	address_space                                *m_data;
	memory_access_cache<2, 0, ENDIANNESS_LITTLE> *m_cache;
	sound_stream *m_stream;
	channel_t m_channel[128];
	std::unique_ptr<s32[]> m_fm_stack[2];
	std::unique_ptr<s32[]> m_am_stack[2];
	u16 m_fm_stack_addr;
	u16 m_am_stack_addr;
	s16 m_dac[2];
};

DECLARE_DEVICE_TYPE(MATRIX32GS_SOUND, matrix32gs_sound_device)

//sound/matrix32gs.cpp
DEFINE_DEVICE_TYPE(MATRIX32GS_SOUND, matrix32gs_sound_device, "matrix32gs_sound", "MATRIX32GS Superfast (Sound)")

//**************************************************************************
//  LIVE DEVICE
//**************************************************************************

//-------------------------------------------------
//  matrix32gs_sound_device - constructor
//-------------------------------------------------

matrix32gs_sound_device::matrix32gs_sound_device(const machine_config & mconfig, const char * tag, device_t * owner, u32 clock)
	: device_t(mconfig, MATRIX32GS_SOUND, tag, owner, clock)
	, device_sound_interface(mconfig, *this)
	, device_memory_interface(mconfig, *this)
	, m_data_config("data", ENDIANNESS_LITTLE, 32, 32)
	, m_stream(nullptr)
{
}


//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void matrix32gs_sound_device::device_start()
{
	m_data = &space(0);
	// Find our direct access
	m_cache = space().cache<2, 0, ENDIANNESS_LITTLE>();

	/* allocate the stream */
	m_stream = stream_alloc(0, 2, clock() / 2048);
	for (int i = 0; i < 2; i++)
	{
		m_fm_stack[i] = make_unique_clear<s32[]>(512);
		m_am_stack[i] = make_unique_clear<s32[]>(512);
		save_pointer(NAME(m_fm_stack[i]), 512, i);
		save_pointer(NAME(m_am_stack[i]), 512, i);
		save_item(NAME(m_dac[i]), i);
	}

	for (int ch = 0; ch < 128; ch++)
	{
		save_item(NAME(m_channel[ch].is_keyon), ch);
		save_item(NAME(m_channel[ch].reg), ch);
		save_item(NAME(m_channel[ch].start), ch);
		save_item(NAME(m_channel[ch].loop), ch);
		save_item(NAME(m_channel[ch].loopend), ch);
		save_item(NAME(m_channel[ch].end), ch);
		save_item(NAME(m_channel[ch].freq), ch);
		save_item(NAME(m_channel[ch].pos), ch);
		save_item(NAME(m_channel[ch].frac), ch);
		save_item(NAME(m_channel[ch].loopmode), ch);
		save_item(NAME(m_channel[ch].lvol), ch);
		save_item(NAME(m_channel[ch].rvol), ch);
		save_item(NAME(m_channel[ch].mvol), ch);
		save_item(NAME(m_channel[ch].pitch), ch);
		save_item(NAME(m_channel[ch].reverse), ch);
		save_item(NAME(m_channel[ch].interpolate), ch);
		save_item(NAME(m_channel[ch].prev_sample), ch);
		save_item(NAME(m_channel[ch].curr_sample), ch);
	}
	save_item(NAME(m_fm_stack_addr));
	save_item(NAME(m_am_stack_addr));
}

//-------------------------------------------------
//  device_clock_changed
//-------------------------------------------------

void matrix32gs_sound_device::device_clock_changed()
{
	m_stream->set_sample_rate(clock() / 2048);
}

//-------------------------------------------------
//  memory_space_config - return a description of
//  any address spaces owned by this device
//-------------------------------------------------

device_memory_interface::space_config_vector matrix32gs_sound_device::memory_space_config() const
{
	return space_config_vector{ std::make_pair(0, &m_data_config) };
}

//-------------------------------------------------
//  sound_stream_update - handle a stream update
//-------------------------------------------------

void matrix32gs_sound_device::sound_stream_update(sound_stream &stream, stream_sample_t **inputs, stream_sample_t **outputs, int samples)
{
	std::fill_n(&outputs[0][0], samples, m_dac[0]);
	std::fill_n(&outputs[1][0], samples, m_dac[1]);
}

void matrix32gs_sound_device::channel_t::keyon()
{
	reverse = 0;
	pos = 0;
	frac = 0;
	std::fill(std::begin(prev_sample), std::end(prev_sample), 0);
	envelope.keyon();
	volume_lfo.keyon();
	pitch_lfo.keyon();
	is_keyon = true;
}

void matrix32gs_sound_device::envelope_t::keyon()
{
	status = 1;
	curr_vol = init_vol;
	curr_rate = delay_rate;
	curr_step = delay_step;
}

void matrix32gs_sound_device::lfo_t::keyon()
{
	status = 1;
	curr_vol = 0;
	attack_scale = 0;
	delay_remain = delay_rate;
	sign = invert;
}

void matrix32gs_sound_device::subeg_t::keyon()
{
	hold = false;
	invert_counter = invert;
	sign_counter = sign;
	counter = 0;
	curr_vol = (invert_counter ? 0x3fffffff - (counter & 0x3fffffff) : (counter & 0x3fffffff)) >> 14;
}

void matrix32gs_sound_device::channel_t::keyoff(bool release)
{
	if (release && (reg & 0x20000) && (envelope.status != 7))
	{
		envelope.status = 7;
		envelope.curr_rate = envelope.release_rate;
	}
	else
	{
		is_keyon = false;
	}
}

// fm/am tick
void matrix32gs_sound_device::fm_am_t::tick(u16 addr, s32 *buffer_l, s32 *buffer_r)
{
	for (int out = 0; out < 2; out++)
	{
		output[out] = 0;
	}
	if (enable)
	{
		int i = 0;
		for (; i <= (input_no & 3); i++)
		{
			output[0] += buffer_l[(addr + inputoffs[i]) & 0x1ff];
			output[1] += buffer_r[(addr + inputoffs[i]) & 0x1ff];
		}
		for (int out = 0; out < 2; out++)
		{
			output[out] <<= 0xf;
			output[out] >>= 0x1f - scale;
			if (i > 0)
				output[out] /= i;
		}
	}
}

// envelope tick
// return : keyon (true = on, false = off)
bool matrix32gs_sound_device::envelope_t::tick()
{
	if (status == 1) // delay
	{
		if (curr_rate)
		{
			curr_vol += curr_step;
			curr_step += delay_inc;
			curr_rate--;
		}
		else
		{
			status = 2; // attack
			curr_rate = attack_rate;
			curr_step = attack_step;
		}
	}
	if (status == 2) // attack
	{
		if (curr_rate)
		{
			curr_vol += curr_step;
			curr_step += attack_inc;
			curr_rate--;
		}
		else
		{
			status = 3; // hold
			curr_rate = hold_rate;
			curr_step = hold_step;
		}
	}
	if (status == 2) // hold
	{
		if (curr_rate)
		{
			curr_vol += curr_step;
			curr_step += hold_inc;
			curr_rate--;
		}
		else
		{
			status = 3; // decay
			curr_rate = decay_rate;
			curr_step = decay_step;
		}
	}
	if (status == 3) // decay
	{
		if (curr_rate)
		{
			curr_vol += curr_step;
			curr_step += decay_inc;
			curr_rate--;
		}
		else
		{
			status = 4; // sustain
			curr_rate = sustain_rate;
			curr_step = sustain_step;
		}
	}
	if (status == 4) // sustain
	{
		if (curr_rate)
		{
			curr_vol += curr_step;
			curr_step += sustain_inc;
			curr_rate--;
		}
		else
		{
			status = 0; // idle
		}
	}
	if (status == 7) // release
	{
		if (curr_rate || (curr_vol > 0))
		{
			curr_vol -= release_step;
			curr_rate--;
		}
		else
		{
			return false;
		}
	}
	curr_vol = std::max(s64(0), std::min(s64((1 << 32) - 1), curr_vol));
	return true;
}

void matrix32gs_sound_device::lfo_t::tick()
{
	if (status == 1) // delay
	{
		if (delay_remain)
		{
			delay_remain--;
			return;
		}
		else
		{
			status = 2; // attack
			attack_remain = attack_rate;
			curr_attack_step = attack_step;
			curr_rate = loop_rate;
			curr_step = sign ? -loop_step : loop_step;
		}
	}

	if (status == 2) // attack
	{
		if (attack_remain)
		{
			attack_scale += curr_attack_step;
			curr_attack_step += attack_inc;
			attack_remain--;
		}
		else
		{
			status = 0; // idle
		}
	}

	attack_scale = std::max(s64(-((1 << 31) - 1)), std::min(s64((1 << 31) - 1), attack_scale));

	curr_vol += curr_step;
	curr_step = sign ? curr_step - loop_inc : curr_step + loop_inc;
	if ((sign && curr_vol > 0 && curr_step < 0) || (!sign && curr_vol < 0 && curr_step > 0))
	{
		wait = true;
	}

	if (curr_rate && !wait)
	{
		curr_rate--;
		if (!curr_rate)
		{
			curr_rate = loop_rate;
			if (!pingpong)
				curr_vol = -curr_vol;
			else
				curr_step = -curr_step;

			if (sign) sign = false;
			else if (!sign) sign = true;
			wait = true;
		}
	}
	if (sign && curr_vol > 0 && curr_step > 0)
	{
		curr_rate = loop_rate;
		sign = false;
		wait = false;
	}
	if (!sign && curr_vol < 0 && curr_step < 0)
	{
		curr_rate = loop_rate;
		sign = true;
		wait = false;
	}
	switch (wave)
	{
		case 0: // off
		case 1: // off
			output = 0;
			break;
		case 2: // sine / triangle
			output = s64(attack_scale * std::max(s64(-((1 << 31) - 1)), std::min(s64((1 << 31) - 1), curr_vol))) >> 31;
			break;
		case 3: // square
			output = (curr_vol == 0) ? 0 : (curr_vol > 0) ? attack_scale : -attack_scale;
			break;
	}
}

void matrix32gs_sound_device::filter_t::tick(s32 &left, s32 &right)
{
	// Filter cycle : A - B - C - D - A - ...
	if (filter_a_en)
	{
		if (filter_a_dir) // hi-pass
		{
			left = left - history_l[4|3] + (filter_a * history_l[0|0]) / 131072 + history_l[0|0] / 2;
			right = right - history_r[4|3] + (filter_a * history_r[0|0]) / 131072 + history_r[0|0] / 2;
		}
		else // low-pass
		{
			left = (filter_a * (left - history_l[0|0]) / 65536) + history_l[0|0];
			right = (filter_a * (right - history_r[0|0]) / 65536) + history_r[0|0];
		}

	}
	history_l[4|0] = history_l[0|0];
	history_r[4|0] = history_r[0|0];
	history_l[0|0] = left;
	history_r[0|0] = right;

	if (filter_b_en)
	{
		if (filter_b_dir) // hi-pass
		{
			left = left - history_l[4|0] + (filter_b * history_l[0|1]) / 131072 + history_l[0|1] / 2;
			right = right - history_r[4|0] + (filter_b * history_r[0|1]) / 131072 + history_r[0|1] / 2;
		}
		else // low-pass
		{
			left = (filter_b * (left - history_l[0|1]) / 65536) + history_l[0|1];
			right = (filter_b * (right - history_r[0|1]) / 65536) + history_r[0|1];
		}

	}
	history_l[4|1] = history_l[0|1];
	history_r[4|1] = history_r[0|1];
	history_l[0|1] = left;
	history_r[0|1] = right;

	if (filter_c_en)
	{
		if (filter_c_dir) // hi-pass
		{
			left = left - history_l[4|1] + (filter_c * history_l[0|2]) / 131072 + history_l[0|2] / 2;
			right = right - history_r[4|1] + (filter_c * history_r[0|2]) / 131072 + history_r[0|2] / 2;
		}
		else // low-pass
		{
			left = (filter_c * (left - history_l[0|2]) / 65536) + history_l[0|2];
			right = (filter_c * (right - history_r[0|2]) / 65536) + history_r[0|2];
		}

	}
	history_l[4|2] = history_l[0|2];
	history_r[4|2] = history_r[0|2];
	history_l[0|2] = left;
	history_r[0|2] = right;

	if (filter_d_en)
	{
		if (filter_d_dir) // hi-pass
		{
			left = left - history_l[4|2] + (filter_d * history_l[0|3]) / 131072 + history_l[0|3] / 2;
			right = right - history_r[4|2] + (filter_d * history_r[0|3]) / 131072 + history_r[0|3] / 2;
		}
		else // low-pass
		{
			left = (filter_d * (left - history_l[0|3]) / 65536) + history_l[0|3];
			right = (filter_d * (right - history_r[0|3]) / 65536) + history_r[0|3];
		}

	}
	history_l[4|3] = history_l[0|3];
	history_r[4|3] = history_r[0|3];
	history_l[0|3] = left;
	history_r[0|3] = right;
}

void matrix32gs_sound_device::subeg_t::tick()
{
	if (!enable)
		return;

	if (hold)
		counter = 0x3fffffff;
	else
	{
		counter += rate;
		if ((shape & 6) == 6)
		{
			sign_counter = sign ^ (BIT(counter, 31) ^ BIT(counter, 30));
		}
		else if ((shape & 6) == 4)
		{
			sign_counter = sign ^ BIT(counter, 30);
		}
		else if ((shape & 6) == 2)
		{
			invert_counter = invert ^ BIT(counter, 30);
		}

		if (shape & 1)
		{
			if (counter > 0x3fffffff)
			{
				counter = 0x3fffffff;
				hold = true;
			}
		}
	}

	curr_vol = (invert_counter ? 0x3fffffff - (counter & 0x3fffffff) : (counter & 0x3fffffff)) >> 14;
	if (sign_counter)
		curr_vol = -curr_vol;

}

// includes/matrix32.h
static constexpr u32 SOUND_CLOCK = 134'217'728; // DSC8123AI5
static constexpr u32 VIDEO_CLOCK = 400'000'000; // DSC8123AI5

class matrix32_state : public driver_device
{
public:
	matrix32_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_maincpu(*this, "maincpu")
		, m_ram(*this, "mainram")
	{ }

	void matrix32(machine_config &config);

private:
	required_device<cpu_device> m_maincpu;

	bitmap_rgb32 m_bitmap;
	std::unique_ptr<uint8_t[]> m_vram; // MT52L256M32D1PF-093 WT:B TR 1GB
	std::unique_ptr<uint64_t[]> m_vram_cache[8];
	u8 m_vram_cache_en;
	u32 m_vram_size;
	required_shared_ptr<u32> m_ram; // 2 * IS42S16160G-5BL

	u8 soundram_r(offs_t offset)
	{
		const u8 ret = read_byte(8, offset);
		m_delay = 0;
		return ret;
	}
	u32 vram_r(offs_t offset, u32 mem_mask = ~0)
	{
		u32 ret = 0;
		if (ACCESSING_BITS_0_7)
			ret |= read_byte(7, offset * 4);
		if (ACCESSING_BITS_8_15)
			ret |= read_byte(7, offset * 4 + 1) << 8;
		if (ACCESSING_BITS_16_23)
			ret |= read_byte(7, offset * 4 + 2) << 16;
		if (ACCESSING_BITS_24_31)
			ret |= read_byte(7, offset * 4 + 3) << 24;

		m_delay = 0;
		return ret;
	}
	void vram_w(offs_t offset, u32 data, u32 mem_mask = ~0)
	{
		if (ACCESSING_BITS_0_7)
			write_byte(offset * 4, data & 0xff);
		if (ACCESSING_BITS_8_15)
			write_byte(offset * 4 + 1, (data >> 8) & 0xff);
		if (ACCESSING_BITS_16_23)
			write_byte(offset * 4 + 2, (data >> 16) & 0xff);
		if (ACCESSING_BITS_24_31)
			write_byte(offset * 4 + 3, (data >> 24) & 0xff);

		m_delay = 0;
	}
	u32 screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);

	inline u8 read_byte(int cache_slot, offs_t offset);
	inline u16 read_word(int cache_slot, offs_t offset);
	inline u32 read_dword(int cache_slot, offs_t offset);
	inline void write_byte(offs_t offset, u8 data);
	inline void write_word(offs_t offset, u16 data);
	inline void write_dword(offs_t offset, u32 data);
	inline u32 get_color(int cache_slot, u32 offs, u64 addr, u8 format);
	inline s32 read_z(u32 offs, u64 addr, u8 format);
	void execute();
	u32 m_delay;
	emu_timer *m_blitter_timer;
	bool m_blitter_busy;

	virtual void machine_reset() override;

	void matrix32_map(address_map &map);
};

// video/matrix32.cpp
inline u8 matrix32_state::read_byte(int cache_slot, offs_t offset)
{
	if (!machine().side_effects_disabled() && (cache_slot < 8))
	{
		if (BIT(m_vram_cache_en, cache_slot))
		{
			offset %= m_vram_size;
			const u16 cache_addr = offset & 0xff;
			const u32 cache_tag = offset & ~0xff;
			const u8 ret = m_vram[offset];
			if (m_vram_cache[cache_slot][cache_addr] != ((1 << 63) | cache_tag | ret))
			{
				m_vram_cache[cache_slot][cache_addr] = ((1 << 63) | cache_tag | ret);
				m_delay++;
			}
			return ret;
		}
		m_delay++;
	}
	return m_vram[offset % m_vram_size];
}

inline u16 matrix32_state::read_word(int cache_slot, offs_t offset)
{
	return read_byte(cache_slot, offset ^ 0) | (read_byte(cache_slot, offset ^ 1) << 8);
}

inline u32 matrix32_state::read_dword(int cache_slot, offs_t offset)
{
	return read_byte(cache_slot, offset ^ 0) |
			(read_byte(cache_slot, offset ^ 1) << 8) |
			(read_byte(cache_slot, offset ^ 2) << 16) |
			(read_byte(cache_slot, offset ^ 3) << 24);
}

inline void matrix32_state::write_byte(offs_t offset, u8 data)
{
	offset %= m_vram_size;
	if (m_vram[offset] != data)
	{
		m_vram[offset] = data;
		if (!machine().side_effects_disabled())
		{
			m_delay++;
			for (int i = 0; i < 8; i++)
			{
				if (BIT(m_vram_cache_en, i))
				{
					const u16 cache_addr = offset & 0xff;
					const u32 cache_tag = offset & ~0xff;
					if (((m_vram_cache[i][cache_addr] & ~(1 << 63)) & ~0xff) == cache_tag)
						m_vram_cache[i][cache_addr] = cache_tag | data;
				}
			}
		}
	}
}

inline void matrix32_state::write_word(offs_t offset, u16 data)
{
	write_byte(offset ^ 0, data & 0xff);
	write_byte(offset ^ 1, data >> 8);
}

inline void matrix32_state::write_dword(offs_t offset, u32 data)
{
	write_byte(offset ^ 0, data & 0xff);
	write_byte(offset ^ 1, data >> 8);
	write_byte(offset ^ 2, data >> 16);
	write_byte(offset ^ 3, data >> 24);
}

static inline u64 get_addr(u32 x, u32 y, u32 width, u32 height, u32 mulwidth, u32 mulheight, bool swapxy, bool flipx, bool flipy)
{
	if (flipx)
		x = width - 1 - x;
	if (flipy)
		y = height - 1 - y;

	if ((x >= mulwidth) || (y >= mulheight))
		return u64(-1);

	u64 addr;
	if (swapxy)
		addr = y + (x * mulheight);
	else
		addr = x + (y * mulwidth);

	return addr;
}

static inline bool atest_check(u8 format, u8 atest, u8 condition, u8 alphamask)
{
	atest &= alphamask;
	condition &= alphamask;
	if (format & 0x10)
		condition ^= alphamask;
	if (format & 8)
		atest ^= alphamask;

	switch (format & 7)
	{
		case 0:
			return false; // always
		case 1:
			return true; // never
		case 2:
			return atest != condition; // equals
		case 3:
			return atest == condition; // nequals
		case 4:
			return atest >= condition; // less
		case 5:
			return atest > condition; // lesseq
		case 6:
			return atest <= condition; // great
		case 7:
			return atest < condition; // greateq
	}
}

static inline bool z_check(u8 format, s32 dstz, s32 srcz, u32 zmask)
{
	srcz &= zmask;
	dstz &= zmask;
	if (format & 0x10)
		srcz ^= zmask;
	if (format & 8)
		dstz ^= zmask;

	switch (format & 7)
	{
		case 0:
			return false; // always
		case 1:
			return true; // never
		case 2:
			return dstz != srcz; // equals
		case 3:
			return dstz == srcz; // nequals
		case 4:
			return dstz >= srcz; // less
		case 5:
			return dstz > srcz; // lesseq
		case 6:
			return dstz <= srcz; // great
		case 7:
			return dstz < srcz; // greateq
	}
}

inline u32 matrix32_state::get_color(int cache_slot, u32 offs, u64 addr, u8 format)
{
	u32 pix = 0;
	switch (format & 3)
	{
		case 0: // ARGB4444
			pix = read_word(cache_slot, offs + (addr << 1));
			pix = rgb_t(pal4bit(pix >> 12), pal4bit(pix >> 8), pal4bit(pix >> 4), pal4bit(pix));
			break;
		case 1: // ARGB1555
			pix = read_word(cache_slot, offs + (addr << 1));
			pix = rgb_t(pal1bit(pix >> 15), pal5bit(pix >> 10), pal5bit(pix >> 5), pal5bit(pix));
			break;
		case 2: // RGB565
			pix = read_word(cache_slot, offs + (addr << 1));
			pix = rgb_t(pal5bit(pix >> 11), pal6bit(pix >> 5), pal5bit(pix));
			break;
		case 3: // ARGB8888
			pix = read_dword(cache_slot, offs + (addr << 2));
			break;
	}
	return pix;
}

inline s32 matrix32_state::read_z(u32 offs, u64 addr, u8 format)
{
	s32 z = 0;
	switch (format & 3)
	{
		case 0: // 8 bit Z value
			z = s8(read_byte(6, offs + addr)) << 24;
			break;
		case 1: // 16 bit Z value
			z = s16(read_word(6, offs + (addr << 1))) << 16;
			break;
		case 2:
		case 3: // 32 bit Z value
			z = s32(read_dword(6, offs + (addr << 2)));
			break;
	}
	return z;
}

static inline void do_zmask(s32 &z, u8 format, u32 &zmask)
{
	zmask = 0xffffffff;
	switch (format)
	{
		case 0:
			zmask &= 0xff000000;
			break;
		case 1:
			zmask &= 0xffff0000;
			break;
	}
	z &= zmask;
}

static inline s16 get_blend(s16 res, u32 src, u32 dst, u32 ext, u8 op, u8 shift)
{
	if (!(op & 0x80)) // bit 7 : enable
		return res;

	if (op & 0x40) // bit 6 : invert constant color input
	{
		ext ^= 0xffffffff;
	}
	if (op & 0x20) // bit 5 : invert source color input
	{
		src ^= 0xffffffff;
	}
	if (op & 0x10) // bit 4 : invert destination color input
	{
		dst ^= 0xffffffff;
	}
	u8 sa = (src >> 24) & 0xff;
	u8 da = (dst >> 24) & 0xff;
	u8 ea = (ext >> 24) & 0xff;
	u8 f = std::min((int)sa, 0xff - da);
	u32 mul = 0;
	switch (op & 7)
	{
		case 0: mul = 0;                                        break; // zero
		case 1: mul = src;                                      break; // srccolor
		case 2: mul = (sa << 24) | (sa << 16) | (sa << 8) | sa; break; // srcalpha
		case 3: mul = dst;                                      break; // dstcolor
		case 4: mul = (da << 24) | (da << 16) | (da << 8) | da; break; // dstalpha
		case 5: mul = ext;                                      break; // constcolor
		case 6: mul = (da << 24) | (da << 16) | (da << 8) | da; break; // constalpha
		case 7: mul = (0xff << 24) | (f << 16) | (f << 8) | f;  break; // srcalphasat
	}
	if (op & 8) // bit 3 : one(invert output)
	{
		mul ^= 0xffffffff;
	}
	return res * ((mul >> shift) & 0xff) / 0xff;
}

static inline s16 do_blend(s16 src, s16 dst, u8 op)
{
	if (!(op & 0x80)) // bit 7 : enable
		return src;

	if (op & 0x20) // bit 5 : invert source color input
	{
		src ^= 0xff;
	}
	if (op & 0x10) // bit 4 : invert destination color input
	{
		dst ^= 0xff;
	}
	switch (op & 7)
	{
		case 0: src = std::min(src + dst, 0xff); break; // additive
		case 1: src = std::max(src - dst, 0);    break; // subtract
		case 2: src = std::max(dst - src, 0);    break; // reversesubtract
		case 3: src = std::min(src, dst);        break; // min
		case 4: src = std::max(src, dst);        break; // max
		case 5: src = (src * dst) / 0xff;        break; // multiply
		case 6: src = (src + dst) >> 1;          break; // average
		case 7: src = abs(src - dst);            break; // difference
	}
	if (op & 8) // bit 3 : invert output
	{
		src ^= 0xff;
	}
	return src;
}

static inline u8 do_logical(u8 src, u8 dst, u8 op)
{
	if (!(op & 0x80)) // bit 7 : enable
		return src;

	if (op & 0x20) // bit 5 : invert source color input
	{
		src ^= 0xff;
	}
	if (op & 0x10) // bit 4 : invert destination color input
	{
		dst ^= 0xff;
	}
	switch (op & 7)
	{
		case 0:
		default: src = src;       break; // copy
		case 1:  src = dst;       break; // noop
		case 2:  src = 0;         break; // clear
		case 3:  src = 0xff;      break; // set
		case 4:  src = src & dst; break; // and
		case 5:  src = src ^ dst; break; // xor
		case 6:  src = src | dst; break; // or
	}
	if (op & 8) // bit 3 : invert output
	{
		src ^= 0xff;
	}
	return src;
}

void matrix32_state::execute()
{
	// destination position
	u32 xoffs;
	u32 yoffs;
	u32 basex;
	u32 basey;
	u32 dstx_per_x;
	u32 dsty_per_x;
	u32 dstx_per_y;
	u32 dsty_per_y;
	u32 dstx_per_x_per_x;
	u32 dstx_per_x_per_y;
	u32 dsty_per_x_per_x;
	u32 dsty_per_x_per_y;
	u32 dstx_per_y_per_x;
	u32 dstx_per_y_per_y;
	u32 dsty_per_y_per_x;
	u32 dsty_per_y_per_y;
	bool wrap_dstx;
	bool wrap_dsty;
	bool dst_flipx;
	bool dst_flipy;
	bool dst_swapxy;
	// destination pictures
	u32 dstoffs;
	u8 dstformat;
	u8 dstxshift;
	u8 dstyshift;
	u32 dstmulwidth;
	u32 dstmulheight;
	u32 dstwidth = (1 << dstxshift);
	u32 dstheight = (1 << dstyshift);
	u32 dstxmask = dstwidth - 1;
	u32 dstymask = dstheight - 1;
	// source position
	u32 startx;
	u32 starty;
	u32 srcx_per_x;
	u32 srcy_per_x;
	u32 srcx_per_y;
	u32 srcy_per_y;
	u32 srcx_per_x_per_x;
	u32 srcx_per_x_per_y;
	u32 srcy_per_x_per_x;
	u32 srcy_per_x_per_y;
	u32 srcx_per_y_per_x;
	u32 srcx_per_y_per_y;
	u32 srcy_per_y_per_x;
	u32 srcy_per_y_per_y;
	bool wrap_srcx;
	bool wrap_srcy;
	bool src_flipx;
	bool src_flipy;
	bool src_swapxy;
	// source pictures
	u32 srcoffs;
	u8 srcformat;
	u32 srcwidth;
	u32 srcheight;
	u32 srcpagexshift;
	u32 srcpageyshift;
	u32 srcpagewidth = 1 << srcpagexshift;
	u32 srcpageheight = 1 << srcpageyshift;
	// draw parameters
	u32 drawwidth;
	u32 drawheight;
	bool clip_in_en;
	bool clip_out_en;
	rectangle clip_in;
	rectangle clip_out;
	bool rowscroll;
	bool colscroll;
	u32 rowaddr;
	u32 coladdr;
	u32 clutoffs;
	u8 clutformat;
	bool indirect_clut_en;
	u8 indirect_clut_format;
	u32 indirect_clut_offs;
	// transparent
	bool opaque;
	u32 transpen;
	u32 transmask;
	// alpha test
	u8 atest;
	u8 atest_format;
	// zbuffer
	bool zread;
	bool zwrite;
	u8 zcheck;
	s32 srcz;
	u32 srczoffs;
	u8 srczformat;
	u32 dstzoffs;
	u8 dstzformat;
	// shade
	bool shade;
	u32 shade_col;
	u32 tint_toggle;
	// blend
	bool blend;
	u8 blendop_a;
	u8 blendop_r;
	u8 blendop_g;
	u8 blendop_b;
	u8 srcblend_a;
	u8 srcblend_r;
	u8 srcblend_g;
	u8 srcblend_b;
	u8 dstblend_a;
	u8 dstblend_r;
	u8 dstblend_g;
	u8 dstblend_b;
	u32 extcolor;
	// logical
	bool logical_en;
	u8 logicalop_a;
	u8 logicalop_r;
	u8 logicalop_g;
	u8 logicalop_b;

	for (int drawy = 0; drawy < drawheight; drawy++)
	{
		for (int drawx = 0; drawx < drawwidth; drawx++)
		{
			// destination position calcutation
			u32 dstx, dsty;
			if (dstx_per_x_per_x == 0 && dstx_per_x_per_y == 0
			 && dsty_per_x_per_x == 0 && dsty_per_x_per_y == 0
			 && dstx_per_y_per_x == 0 && dstx_per_y_per_y == 0
			 && dsty_per_y_per_x == 0 && dsty_per_y_per_y == 0)
			{
				if (dstx_per_x == 0x10000 && dstx_per_y == 0
				 && dsty_per_x == 0 && dsty_per_y == 0x10000)
				{
					dstx = (basex >> 16) + drawx;
					dsty = (basey >> 16) + drawy;
				}
				else
				{
					dstx = basex + (dstx_per_x * drawx) + (dstx_per_y * drawy);
					dsty = basey + (dsty_per_x * drawx) + (dsty_per_y * drawy);
					dstx >>= 16;
					dsty >>= 16;
				}
			}
			else
			{
				u32 res_dstx_per_x = dstx_per_x + (dstx_per_x_per_x * drawx) + (dstx_per_x_per_y * drawy);
				u32 res_dsty_per_x = dsty_per_x + (dsty_per_x_per_x * drawx) + (dsty_per_x_per_y * drawy);
				u32 res_dstx_per_y = dstx_per_y + (dstx_per_y_per_x * drawx) + (dstx_per_y_per_y * drawy);
				u32 res_dsty_per_y = dsty_per_y + (dsty_per_y_per_x * drawx) + (dsty_per_y_per_y * drawy);

				dstx = basex + (res_dstx_per_x * drawx) + (res_dstx_per_y * drawy);
				dsty = basey + (res_dsty_per_x * drawx) + (res_dsty_per_y * drawy);
				dstx >>= 16;
				dsty >>= 16;
			}
			dstx += xoffs;
			dsty += yoffs;
			if (wrap_dstx)
				dstx &= dstxmask;

			if (wrap_dsty)
				dsty &= dstymask;

			if ((!clip_in_en || clip_in.contains(dstx, dsty)) && (!clip_out_en || !clip_out.contains(dstx, dsty)))
			{
				// source position calculation
				u32 srcx, srcy;
				if (srcx_per_x_per_x == 0 && srcx_per_x_per_y == 0
				 && srcy_per_x_per_x == 0 && srcy_per_x_per_y == 0
				 && srcx_per_y_per_x == 0 && srcx_per_y_per_y == 0
				 && srcy_per_y_per_x == 0 && srcy_per_y_per_y == 0)
				{
					if (srcx_per_x == 0x10000 && srcx_per_y == 0
					 && srcy_per_x == 0 && srcy_per_y == 0x10000)
					{
						if (!rowscroll && !colscroll)
						{
							srcx = (startx >> 16) + drawx;
							srcy = (starty >> 16) + drawy;
						}
						else
						{
							srcx = startx + (drawx << 16);
							srcy = starty + (drawy << 16);

							if (rowscroll)
							{
								srcx += read_dword(0, rowaddr + (drawy * 4) + 0) + (read_dword(0, rowaddr + (drawy * 4) + 2) * drawx);
								srcy += read_dword(0, rowaddr + (drawy * 4) + 1) + (read_dword(0, rowaddr + (drawy * 4) + 3) * drawx);
							}

							if (colscroll)
							{
								srcx += read_dword(1, coladdr + (drawx * 4) + 0) + (read_dword(1, coladdr + (drawx * 4) + 2) * drawy);
								srcy += read_dword(1, coladdr + (drawx * 4) + 1) + (read_dword(1, coladdr + (drawx * 4) + 3) * drawy);
							}

							srcx >>= 16;
							srcy >>= 16;
						}
					}
					else
					{
						srcx = startx + (srcx_per_x * drawx) + (srcx_per_y * drawy);
						srcy = starty + (srcy_per_x * drawx) + (srcy_per_y * drawy);

						if (rowscroll)
						{
							srcx += read_dword(0, rowaddr + (drawy * 4) + 0) + (read_dword(0, rowaddr + (drawy * 4) + 2) * drawx);
							srcy += read_dword(0, rowaddr + (drawy * 4) + 1) + (read_dword(0, rowaddr + (drawy * 4) + 3) * drawx);
						}

						if (colscroll)
						{
							srcx += read_dword(1, coladdr + (drawx * 4) + 0) + (read_dword(1, coladdr + (drawx * 4) + 2) * drawy);
							srcy += read_dword(1, coladdr + (drawx * 4) + 1) + (read_dword(1, coladdr + (drawx * 4) + 3) * drawy);
						}

						srcx >>= 16;
						srcy >>= 16;
					}
				}
				else
				{
					u32 res_srcx_per_x = srcx_per_x + (srcx_per_x_per_x * drawx) + (srcx_per_x_per_y * drawy);
					u32 res_srcy_per_x = srcy_per_x + (srcy_per_x_per_x * drawx) + (srcy_per_x_per_y * drawy);
					u32 res_srcx_per_y = srcx_per_y + (srcx_per_y_per_x * drawx) + (srcx_per_y_per_y * drawy);
					u32 res_srcy_per_y = srcy_per_y + (srcy_per_y_per_x * drawx) + (srcy_per_y_per_y * drawy);

					srcx = (startx + (res_srcx_per_x * drawx) + (res_srcx_per_y * drawy));
					srcy = (starty + (res_srcy_per_x * drawx) + (res_srcy_per_y * drawy));

					if (rowscroll)
					{
						srcx += read_dword(0, rowaddr + (drawy * 4) + 0) + (read_dword(0, rowaddr + (drawy * 4) + 2) * drawx);
						srcy += read_dword(0, rowaddr + (drawy * 4) + 1) + (read_dword(0, rowaddr + (drawy * 4) + 3) * drawx);
					}

					if (colscroll)
					{
						srcx += read_dword(1, coladdr + (drawx * 4) + 0) + (read_dword(1, coladdr + (drawx * 4) + 2) * drawy);
						srcy += read_dword(1, coladdr + (drawx * 4) + 1) + (read_dword(1, coladdr + (drawx * 4) + 3) * drawy);
					}

					srcx >>= 16;
					srcy >>= 16;
				}
				switch (wrap_srcx)
				{
					case 0:
						if (srcx > srcwidth)
							goto skip_col;

						break;
					case 1:
						srcx %= srcwidth;
						break;
					case 2:
						if (srcx > (srcwidth * 2))
							goto skip_col;

						if (srcx > srcwidth)
							srcx = srcwidth - 1 - (srcx % srcwidth);

						break;
					case 3:
						srcx %= srcwidth * 2;
						if (srcx > srcwidth)
							srcx = srcwidth - 1 - (srcx % srcwidth);
						break;
				}
				switch (wrap_srcy)
				{
					case 0:
						if (srcy > srcheight)
							goto skip_col;

						break;
					case 1:
						srcy %= srcheight;
						break;
					case 2:
						if (srcy > (srcheight * 2))
							goto skip_col;

						if (srcy > srcheight)
							srcy = srcheight - 1 - (srcy % srcheight);

						break;
					case 3:
						srcy %= srcheight * 2;
						if (srcy > srcheight)
							srcy = srcheight - 1 - (srcy % srcheight);
						break;
				}

				u64 srcaddr = get_addr(srcx, srcy, srcwidth, srcheight, std::max(srcwidth, srcpagewidth), std::max(srcheight, srcpageheight), src_swapxy, src_flipx, src_flipy);

				if (srcaddr == u64(-1))
					goto skip_col;

				// get pixels
				u32 pix;
				bool is_clut = false;
				switch (srcformat)
				{
					case 0: // 1bpp CLUT
						pix = BIT(read_byte(2, srcoffs + (srcaddr >> 3)), 7 - (srcaddr & 7));
						is_clut = true;
						break;
					case 1: // 2bpp CLUT
						pix = (read_byte(2, srcoffs + (srcaddr >> 2)) >> ((3 - (srcaddr & 3)) << 1)) & 3;
						is_clut = true;
						break;
					case 2: // 4bpp CLUT
						pix = (read_byte(2, srcoffs + (srcaddr >> 1)) >> ((1 - (srcaddr & 1)) << 2)) & 0xf;
						is_clut = true;
						break;
					case 3: // 8bpp CLUT
						pix = read_byte(2, srcoffs + srcaddr);
						is_clut = true;
						break;
					case 4: // ARGB4444
						pix = read_word(2, srcoffs + (srcaddr << 1));
						pix = rgb_t(pal4bit(pix >> 12), pal4bit(pix >> 8), pal4bit(pix >> 4), pal4bit(pix));
						transmask = 0xf0f0f0f0;
						break;
					case 5: // ARGB1555
						pix = read_word(2, srcoffs + (srcaddr << 1));
						pix = rgb_t(pal1bit(pix >> 15), pal5bit(pix >> 10), pal5bit(pix >> 5), pal5bit(pix));
						transmask = 0x80f8f8f8;
						break;
					case 6: // RGB565
						pix = read_word(2, srcoffs + (srcaddr << 1));
						pix = rgb_t(pal5bit(pix >> 11), pal6bit(pix >> 5), pal5bit(pix));
						transmask = 0xf8fcf8;
						break;
					case 7: // ARGB8888
						pix = read_dword(2, srcoffs + (srcaddr << 2));
						transmask = 0xffffffff;
						break;
				}
				if (is_clut)
				{
					if (indirect_clut_en)
					{
						switch (indirect_clut_format)
						{
							case 0: // 8 bit indirect CLUT table
								pix = read_byte(3, indirect_clut_offs + pix);
								break;
							case 1: // 16 bit indirect CLUT table
								pix = read_word(3, indirect_clut_offs + (pix * 2));
								break;
						}
					}
					pix = get_color(4, clutoffs, pix, clutformat);
					switch (clutformat)
					{
						case 0:
							transmask = 0xf0f0f0f0;
							break;
						case 1:
							transmask = 0x80f8f8f8;
							break;
						case 2:
							transmask = 0xf8fcf8;
							break;
						case 3:
							transmask = 0xffffffff;
							break;
					}
				}
				transpen &= transmask;
				if ((!opaque && ((pix & transmask) == transpen)) || (atest_check(atest_format, atest, pix >> 24, transmask >> 24)))
					goto skip_col;

				u64 dstaddr = get_addr(dstx, dsty, dstwidth, dstheight, dstmulwidth, dstmulheight, dst_swapxy, dst_flipx, dst_flipy);

				if (dstaddr == u64(-1))
					goto skip_col;

				u32 dstpix = get_color(5, dstoffs, dstaddr, dstformat);
				// zbuffer processing
				if (srczformat & 4) // zbuffer map case
				{
					srcz = read_z(srczoffs, srcaddr, srczformat);
				}
				else
				{
					switch (srczformat & 3)
					{
						case 0:
							srcz = (s8)srcz << 24;
							break;
						case 1:
							srcz = (s16)srcz << 16;
							break;
					}
				}

				u32 dstzmask;
				do_zmask(srcz, dstzformat, dstzmask);
				if (zread)
				{
					u32 srczmask;
					s32 dstz = read_z(dstzoffs, dstaddr, dstzformat);
					do_zmask(dstz, srczformat, srczmask);
					if (z_check(zcheck, dstz, srcz, srczmask))
						goto skip_col;

				}
				if (zwrite)
				{
					switch (dstzformat)
					{
						case 0:
							write_byte(dstzoffs + dstaddr, srcz >> 24);
							break;
						case 1:
							write_word(dstzoffs + (dstaddr << 1), srcz >> 16);
							break;
						case 2:
						case 3:
							write_dword(dstzoffs + (dstaddr << 2), srcz);
							break;
					}
				}
				// shade / tint
				if (shade)
				{
					s16 a = (pix >> 24) & 0xff;
					s16 r = (pix >> 16) & 0xff;
					s16 g = (pix >>  8) & 0xff;
					s16 b = (pix >>  0) & 0xff;

					u8 sa = (shade_col >> 24) & 0xff;
					u8 sr = (shade_col >> 16) & 0xff;
					u8 sg = (shade_col >>  8) & 0xff;
					u8 sb = (shade_col >>  0) & 0xff;

					// bit 4 : invert shade/tint parameter input
					if (BIT(tint_toggle, 28)) sa ^= 0xff;
					if (BIT(tint_toggle, 20)) sr ^= 0xff;
					if (BIT(tint_toggle, 12)) sg ^= 0xff;
					if (BIT(tint_toggle,  4)) sb ^= 0xff;

					// bit 5 : invert source color input
					if (BIT(tint_toggle, 29)) a ^= 0xff;
					if (BIT(tint_toggle, 21)) r ^= 0xff;
					if (BIT(tint_toggle, 13)) g ^= 0xff;
					if (BIT(tint_toggle,  5)) b ^= 0xff;

					// bit 7 : enable, bit 0 : toggle shade/tint
					if (BIT(tint_toggle, 31)) a = std::max(0, std::min(0xff, BIT(tint_toggle, 24) ? a + (((0xff - a) * sa) / 0xff) : (a * sa) / 0xff));
					if (BIT(tint_toggle, 23)) r = std::max(0, std::min(0xff, BIT(tint_toggle, 16) ? r + (((0xff - r) * sr) / 0xff) : (r * sr) / 0xff));
					if (BIT(tint_toggle, 15)) g = std::max(0, std::min(0xff, BIT(tint_toggle,  8) ? g + (((0xff - g) * sg) / 0xff) : (g * sg) / 0xff));
					if (BIT(tint_toggle,  7)) b = std::max(0, std::min(0xff, BIT(tint_toggle,  0) ? b + (((0xff - b) * sb) / 0xff) : (b * sb) / 0xff));

					// bit 3 : invert output
					if (BIT(tint_toggle, 27)) a ^= 0xff;
					if (BIT(tint_toggle, 19)) r ^= 0xff;
					if (BIT(tint_toggle, 11)) g ^= 0xff;
					if (BIT(tint_toggle,  3)) b ^= 0xff;

					pix = (a << 24) | (r << 16) | (g << 8) | b;
				}
				// blend
				if (blend)
				{
					s16 sa = (pix >> 24) & 0xff;
					s16 sr = (pix >> 16) & 0xff;
					s16 sg = (pix >>  8) & 0xff;
					s16 sb = (pix >>  0) & 0xff;

					s16 da = (dstpix >> 24) & 0xff;
					s16 dr = (dstpix >> 16) & 0xff;
					s16 dg = (dstpix >>  8) & 0xff;
					s16 db = (dstpix >>  0) & 0xff;

					sa = get_blend(sa, pix, dstpix, extcolor, srcblend_a, 24);
					sr = get_blend(sr, pix, dstpix, extcolor, srcblend_r, 16);
					sg = get_blend(sg, pix, dstpix, extcolor, srcblend_g, 8);
					sb = get_blend(sb, pix, dstpix, extcolor, srcblend_b, 0);

					da = get_blend(da, pix, dstpix, extcolor, dstblend_a, 24);
					dr = get_blend(dr, pix, dstpix, extcolor, dstblend_r, 16);
					dg = get_blend(dg, pix, dstpix, extcolor, dstblend_g, 8);
					db = get_blend(db, pix, dstpix, extcolor, dstblend_b, 0);

					sa = do_blend(sa, da, blendop_a);
					sr = do_blend(sr, dr, blendop_r);
					sg = do_blend(sg, dg, blendop_g);
					sb = do_blend(sb, db, blendop_b);

					pix = (sa << 24) | (sr << 16) | (sg << 8) | sb;
				}
				if (logical_en)
				{
					u8 sa = (pix >> 24) & 0xff;
					u8 sr = (pix >> 16) & 0xff;
					u8 sg = (pix >>  8) & 0xff;
					u8 sb = (pix >>  0) & 0xff;

					u8 da = (dstpix >> 24) & 0xff;
					u8 dr = (dstpix >> 16) & 0xff;
					u8 dg = (dstpix >>  8) & 0xff;
					u8 db = (dstpix >>  0) & 0xff;

					sa = do_logical(sa, da, logicalop_a);
					sr = do_logical(sr, dr, logicalop_r);
					sg = do_logical(sg, dg, logicalop_g);
					sb = do_logical(sb, db, logicalop_b);

					pix = (sa << 24) | (sr << 16) | (sg << 8) | sb;
				}
				// write pixel
				switch (dstformat)
				{
					case 0: // ARGB4444
						pix = (pix & 0xf0 >> 4) | (pix & 0xf000 >> 8) | (pix & 0xf00000 >> 12) | (pix & 0xf0000000 >> 16);
						write_word(dstoffs + (dstaddr << 1), pix);
						break;
					case 1: // ARGB1555
						pix = (pix & 0xf8 >> 3) | (pix & 0xf800 >> 6) | (pix & 0xf80000 >> 9) | (pix & 0x80000000 >> 16);
						write_word(dstoffs + (dstaddr << 1), pix);
						break;
					case 2: // RGB565
						pix = (pix & 0xf8 >> 3) | (pix & 0xfc00 >> 5) | (pix & 0xf80000 >> 8);
						write_word(dstoffs + (dstaddr << 1), pix);
						break;
					case 3: // ARGB8888
						write_dword(dstoffs + (dstaddr << 2), pix);
						break;
				}
			}
			// end of processing
skip_col:
			m_delay++;
		}
	}
	m_blitter_busy = true;
	m_blitter_timer->adjust(attotime::from_ticks(m_delay, VIDEO_CLOCK));
}