// license:BSD-3-Clause
// copyright-holders:Nathan Woods, Curt Coder
/**********************************************************************

    MATRIX JKM6581 SID4

    SID with independent waveform from RAM, Independent filter,
    Stereo output, 8 voices, Mirroring mode

**********************************************************************
                            _____   _____
                 CAP1A   1 |*    \_/     | 28  Vdd
                 CAP1B   2 |             | 27  AUDIO LOUT
                 CAP2A   3 |             | 26  AUDIO ROUT
                 CAP2B   4 |             | 25  Vcc
                  _RES   5 |             | 24  A5
                  phi2   6 |             | 23  A6
                  R/_W   7 |   JKM6581   | 22  D7
                   _CS   8 |             | 21  D6
                    A0   9 |             | 20  D5
                    A1  10 |             | 19  D4
                    A2  11 |             | 18  D3
                    A3  12 |             | 17  D2
                    A4  13 |             | 16  D1
                   GND  14 |_____________| 15  D0

**********************************************************************/

#ifndef MAME_SOUND_JKM6581_H
#define MAME_SOUND_JKM6581_H

#pragma once


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

// ======================> jkm6581_device

struct JKM6581_t;

class jkm6581_device : public device_t, public device_sound_interface, public device_memory_interface
{
public:
	// used by the actual SID emulator
	enum
	{
		TYPE_6581,
		TYPE_8580
	};

	jkm6581_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock);
	~jkm6581_device();

	u8 read(offs_t offset);
	void write(offs_t offset, u8 data);

protected:
	// device-level overrides
	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void device_post_load() override;

	// device_sound_interface overrides
	virtual void sound_stream_update(sound_stream &stream, stream_sample_t **inputs, stream_sample_t **outputs, int samples) override;

	// device_memory_interface configuration
	virtual space_config_vector memory_space_config() const override;

	address_space_config m_data_config;
private:
	address_space                                *m_data;
	memory_access_cache<0, 0, ENDIANNESS_LITTLE> *m_cache;

	sound_stream *m_stream;

	std::unique_ptr<JKM6581_t> m_token;
};


// device type definition
DECLARE_DEVICE_TYPE(JKM6581, jkm6581_device)

#endif // MAME_SOUND_JKM6581_H
