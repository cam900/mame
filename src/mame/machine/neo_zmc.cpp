// license:BSD-3-Clause
// copyright-holders:<author_name>
/***************************************************************************

NEO-ZMC Z80 Memory controller emulations
NEO-ZMC2 is Integrated with PRO-CT0
NEO-CMC of later cartridges has similar logics

***************************************************************************/

#include "emu.h"
#include "neo_zmc.h"

#include <algorithm>

//**************************************************************************
//  GLOBAL VARIABLES
//**************************************************************************

// device type definition
DEFINE_DEVICE_TYPE(NEO_ZMC, neo_zmc_device, "neo_zmc", "SNK NEO_ZMC Bankswitch device")


//**************************************************************************
//  LIVE DEVICE
//**************************************************************************

//-------------------------------------------------
//  neo_zmc_device - constructor
//-------------------------------------------------

neo_zmc_device::neo_zmc_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, NEO_ZMC, tag, owner, clock)
	, m_source_size(0x400000) // ZMC2(22bit Address Space)
{
	std::fill(std::begin(m_bank), std::end(m_bank), 0);
}


//-------------------------------------------------
//  set_source - set bankswitched source
//-------------------------------------------------

void neo_zmc_device::set_source(uint8_t *source, uint32_t size)
{
	m_source_size = size;
	std::copy(&source[0], &source[m_source_size], m_source.begin());
	std::fill(std::begin(m_bank), std::end(m_bank), 0);
}


//-------------------------------------------------
//  set_region - set tag of bankswitched memory region
//-------------------------------------------------

void neo_zmc_device::set_region(const char *tag)
{
	if (tag)
	{
		uint8_t *rom = machine().root_device().memregion(tag)->base();
		uint32_t size = machine().root_device().memregion(tag)->bytes();
		set_source(rom, size);
	}
}


//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void neo_zmc_device::device_start()
{
	/*
		Max address bits:
		ZMC, CMC 19bit(M11-M18)
		ZMC2 22bit(M11-M21)
	*/
	m_source.resize(0x400000, 0);
	save_item(NAME(m_source));
	save_item(NAME(m_source_size));
	save_item(NAME(m_bank));
}


//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void neo_zmc_device::device_reset()
{
}


//**************************************************************************
//  READ/WRITE HANDLERS
//**************************************************************************

READ8_MEMBER( neo_zmc_device::bank_r )
{
	m_bank[offset & 3] = (offset >> 8) & 0xff; // high 8 address line is bankswitch select

	return 0;
}

// handler of 0x8000-0xf7ff bankswitched area
// 0x0000-0x7fff : fixed area
READ8_MEMBER( neo_zmc_device::banked_rom_r )
{
	if (offset & 0x4000)
	{
		if (offset & 0x2000)
		{
			if (offset & 0x1000)
			{
				return read_rom(0,offset); // 0xf000-0xf7ff
			}
			return read_rom(1,offset); // 0xe000-0xefff
		}
		return read_rom(2,offset); // 0xc000-0xdfff
	}
	return read_rom(3,offset); // 0x8000-0xbfff
}
