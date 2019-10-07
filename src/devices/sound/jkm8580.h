// license:BSD-3-Clause
// copyright-holders:Nathan Woods, Curt Coder
/**********************************************************************

    MATRIX JKM8580 Sound Interface Device emulation

**********************************************************************
                            _____   _____
                 CAP1A   1 |*    \_/     | 28  Vdd
                 CAP1B   2 |             | 27  AUDIO LOUT
                 CAP2A   3 |             | 26  AUDIO ROUT
                 CAP2B   4 |             | 25  Vcc
                  _RES   5 |             | 24  A6
                  phi2   6 |             | 23  A5
                  R/_W   7 |   JKM8580   | 22  D7
                   _CS   8 |    SID2     | 21  D6
                    A0   9 |             | 20  D5
                    A1  10 |             | 19  D4
                    A2  11 |             | 18  D3
                    A3  12 |             | 17  D2
                    A4  13 |             | 16  D1
                   GND  14 |_____________| 15  D0

**********************************************************************/

#ifndef MAME_SOUND_JKM8580_H
#define MAME_SOUND_JKM8580_H

#pragma once


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

// ======================> jkm8580_device

struct JKM8580_t;

class jkm8580_device : public device_t, public device_sound_interface
{
public:
	// used by the actual SID emulator
	enum
	{
		TYPE_6581,
		TYPE_8580
	};

	jkm8580_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);
	~jkm8580_device();

	uint8_t read(offs_t offset);
	void write(offs_t offset, uint8_t data);

protected:
	// device-level overrides
	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void device_post_load() override;

	// device_sound_interface overrides
	virtual void sound_stream_update(sound_stream &stream, stream_sample_t **inputs, stream_sample_t **outputs, int samples) override;

	void save_state(JKM8580_t *token);
private:
	sound_stream *m_stream;

	std::unique_ptr<JKM8580_t> m_token;
};


// device type definition
DECLARE_DEVICE_TYPE(JKM8580, jkm8580_device)

#endif // MAME_SOUND_JKM8580_H
