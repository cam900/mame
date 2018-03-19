// license:BSD-3-Clause
// copyright-holders:<author_name>
/***************************************************************************

NEO-ZMC Z80 Memory controller

***************************************************************************/

#ifndef MAME_MACHINE_NEO_ZMC_H
#define MAME_MACHINE_NEO_ZMC_H

#pragma once



//**************************************************************************
//  INTERFACE CONFIGURATION MACROS
//**************************************************************************

#define MCFG_NEO_ZMC_ADD(tag, freq) \
		MCFG_DEVICE_ADD((tag), NEO_ZMC, (freq))

#define MCFG_NEO_ZMC_ROM(_tag) \
		downcast<neo_zmc_device &>(*device).set_region(_tag);

//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

// ======================> neo_zmc_device

class neo_zmc_device : public device_t
{
public:
	// construction/destruction
	neo_zmc_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);
	
	// configuration
	void set_source(uint8_t *source, uint32_t size);
	void set_region(const char *tag);

	// I/O operations
	DECLARE_READ8_MEMBER( bank_r );
	DECLARE_READ8_MEMBER( banked_rom_r );

protected:
	// device-level overrides
	//virtual void device_validity_check(validity_checker &valid) const override;
	virtual void device_add_mconfig(machine_config &config) override;
	virtual void device_start() override;
	virtual void device_reset() override;

private:
	uint8_t m_bank[4];
	std::vector<uint8_t> m_source;
	uint32_t m_source_size;

	uint8_t read_rom(int no, offs_t offset) { return m_source[ ( (m_bank[no] << (11 + no)) | (offset & ((1 << (11+no))-1)) ) % m_source_size]; }
};


// device type definition
DECLARE_DEVICE_TYPE(NEO_ZMC, neo_zmc_device)



//**************************************************************************
//  GLOBAL VARIABLES
//**************************************************************************


#endif // MAME_MACHINE_NEO_ZMC_H
