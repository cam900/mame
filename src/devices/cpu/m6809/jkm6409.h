// license:BSD-3-Clause
// copyright-holders:Nathan Woods
/*********************************************************************

    jkm6409.h

    Portable Hitachi 6309 emulator

**********************************************************************/

#ifndef MAME_CPU_M6809_JKM6409_H
#define MAME_CPU_M6809_JKM6409_H

#pragma once

#include "m6809.h"


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

// device type definitions
DECLARE_DEVICE_TYPE(JKM6409, jkm6409_device)

// ======================> jkm6409_device

class jkm6409_device : public m6809_base_device
{
public:
	// construction/destruction
	jkm6409_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

protected:
	class mi_jkm6409 : public memory_interface {
	public:
		address_space *m_io;

		virtual ~mi_jkm6409() {}
		virtual uint8_t read_port(uint16_t adr) override;
		virtual void write_port(uint16_t adr, uint8_t val) override;
	};

	// device-level overrides
	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void device_pre_save() override;
	virtual void device_post_load() override;

	// device_execute_interface overrides
	virtual void execute_run() override;

	// device_memory_interface overrides
	virtual space_config_vector memory_space_config() const override;
	virtual bool memory_translate(int spacenum, int intention, offs_t &address) override;

	virtual offs_t translated(offs_t adr) { return (m_mmr[addr >> 12] << 12) | (addr & 0xfff); } override;

	// device_disasm_interface overrides
	virtual std::unique_ptr<util::disasm_interface> create_disassembler() override;

	virtual bool is_6809() override { return false; };

private:
	typedef m6809_base_device super;
	// address spaces
	const address_space_config  m_io_config;

	// addressing modes
	enum
	{
		ADDRESSING_MODE_REGISTER_E = 5,
		ADDRESSING_MODE_REGISTER_F = 6,
		ADDRESSING_MODE_REGISTER_W = 7,
		ADDRESSING_MODE_REGISTER_X = 8,
		ADDRESSING_MODE_REGISTER_Y = 9,
		ADDRESSING_MODE_REGISTER_U = 10,
		ADDRESSING_MODE_REGISTER_S = 11,
		ADDRESSING_MODE_REGISTER_CC = 12,
		ADDRESSING_MODE_REGISTER_DP = 13,
		ADDRESSING_MODE_REGISTER_PC = 14,
		ADDRESSING_MODE_REGISTER_V = 15,
		ADDRESSING_MODE_ZERO = 16
	};

	// interrupt vectors
	enum
	{
		VECTOR_ILLEGAL = 0xFFF0
	};

	// CPU registers
	PAIR16  m_v;
	PAIR16  m_mmr[16];
	uint8_t   m_md;

	// other state
	uint8_t   m_temp_im;

	// operand reading/writing
	uint8_t read_operand();
	uint8_t read_operand(int ordinal);
	void write_operand(uint8_t data);
	void write_operand(int ordinal, uint8_t data);

	// read a byte from given memory location
	inline uint8_t read_port(uint16_t address)             { eat(2); return m_mintf->read_port(address); }

	// write a byte to given memory location
	inline void write_port(uint16_t address, uint8_t data) { eat(2); m_mintf->write_port(address, data); }

	// interrupt registers
	bool firq_saves_entire_state()      { return m_md & 0x02; }
	uint16_t entire_state_registers()     { return jkm6409_native_mode() ? 0x3FF : 0xFF; }

	// bit tests
	uint8_t &bittest_register();
	bool bittest_source();
	bool bittest_dest();
	void bittest_set(bool result);

	// complex instructions
	void muld();
	bool divq();
	bool divd();

	// miscellaneous
	void set_e()                                    { m_addressing_mode = ADDRESSING_MODE_REGISTER_E; }
	void set_f()                                    { m_addressing_mode = ADDRESSING_MODE_REGISTER_F; }
	void set_w()                                    { m_addressing_mode = ADDRESSING_MODE_REGISTER_W; }
	void set_x()                                    { m_addressing_mode = ADDRESSING_MODE_REGISTER_X; }
	void set_y()                                    { m_addressing_mode = ADDRESSING_MODE_REGISTER_Y; }

	exgtfr_register read_exgtfr_register(uint8_t reg);
	void write_exgtfr_register(uint8_t reg, exgtfr_register value);
	bool tfr_read(uint8_t opcode, uint8_t arg, uint8_t &data);
	bool tfr_write(uint8_t opcode, uint8_t arg, uint8_t data);
	bool add8_sets_h()                              { return (m_opcode & 0xFE) != 0x30; }
	void register_register_op();
	bool hd6309_native_mode()           { return true; }

	void execute_one();
	void set_mmr();
	u16 get_mmr();
};

enum
{
	JKM6409_PC = M6809_PC,
	JKM6409_S = M6809_S,
	JKM6409_CC = M6809_CC,
	JKM6409_A = M6809_A,
	JKM6409_B = M6809_B,
	JKM6409_D = M6809_D,
	JKM6409_U = M6809_U,
	JKM6409_X = M6809_X,
	JKM6409_Y = M6809_Y,
	JKM6409_DP = M6809_DP,

	JKM6409_E = 1000,
	JKM6409_F,
	JKM6409_W,
	JKM6409_Q,
	JKM6409_V,
	JKM6409_MD,
	JKM6409_ZERO_BYTE,
	JKM6409_ZERO_WORD,
	JKM6409_MMR0,
	JKM6409_MMR1,
	JKM6409_MMR2,
	JKM6409_MMR3,
	JKM6409_MMR4,
	JKM6409_MMR5,
	JKM6409_MMR6,
	JKM6409_MMR7,
	JKM6409_MMR8,
	JKM6409_MMR9,
	JKM6409_MMR10,
	JKM6409_MMR11,
	JKM6409_MMR12,
	JKM6409_MMR13,
	JKM6409_MMR14,
	JKM6409_MMR15
};

#define JKM6409_IRQ_LINE  M6809_IRQ_LINE   /* 0 - IRQ line number */
#define JKM6409_FIRQ_LINE M6809_FIRQ_LINE  /* 1 - FIRQ line number */

#endif // MAME_CPU_M6809_JKM6409_H
