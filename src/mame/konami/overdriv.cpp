// license:BSD-3-Clause
// copyright-holders:Nicola Salmoria
/***************************************************************************

    Over Drive (GX789) (c) 1990 Konami

    driver by Nicola Salmoria

    Notes:
    - Main IRQ4 is vblank (scanline 256). IRQ5 is the CCU-style timer.
      Attract $40036 counts completed demo plays (cmpi #6), not IRQ5 ticks.
      Six demo plays ($200001==1, $200003==1) share the gameplay timer:
      IRQ5 at lines 0 and 140 so $400B2 reaches 3 and chip0 ROZ MOVEP runs,
      calibrated against the PCB recording (TIME 0.68/s vs 0.675/s measured).
      Title ($200001==1, $200003>=2) and boot/ROM-check ($200001==0) get one
      IRQ5 per frame. CCU INT-TIME is never rewritten after the boot MOVEM;
      demo↔title only changes work-RAM counters ($40036/$40064/$40032) and
      LVC ctrl bytes ($200200=$69/$79/$39, $200208=$60/$10). IRQ5 is skipped
      while IPL>=5 so a long ISR cannot stack and starve IRQ4.
    - Test mode ROM check also lives on IRQ5; keep 1/frame until $200001!=0.
    - Sub IRQ4 comes from main $230000, IRQ5 ($238000) is GFX ROM check only,
      IRQ6 is K053246 OBJ DMA end — not the main CPU.
    - Sub $140001 is a perspective ALU (cmds $18/$1B) over $20BFxx, not a copy
      trigger. Main $140000 is MOVE.B watchdog (D8-D15). CCU res_change can
      fire extra vblanks, so the watchdog is time-based rather than 8 vblanks.
    - LVC wiring (checked against the sub CPU ROM test and the line data):
      LVC A = regs $100000, ROM window $218000, 3 ROMs (e18/e19/e20), line RAM $C1000
      LVC B = regs $108000, ROM window $220000, 2 ROMs (e17/e16), line RAM $C0000
      LVC B is the road (main CPU prepares it in $201400/$203300, sub copies to
      $C0300), ctrl $60 (SWAP_XY). LVC A draws bridges/walls/tunnels from course
      codes $8xx (sub $5F40), ctrl $79/$39 (FLIP_X|FLIP_Y, no swap).
      Palette: LVC A = CI1, LVC B = CI2. Line word0 bits 8-13 are the 053251
      priority; $3F lines sit behind the skybox ($3E).
    - Origins (visible area at 0,0): K051316 (7,-16) both, K053250 (0,-16) both,
      K053246 (-45,38). The OBJ window register is $FFE0/$FEEA only until the
      first demo; $99EE sets $FFD3/$FEEA every gameplay frame and nothing resets it.
    - The "Continue?" sprites are not visible until you press start
    - priorities

    The issues below are both IRQ timing, and relate to when the sprites get
    copied across by the DMA
    - Some flickering sprites, this might be an interrupt/timing issue
    - The screen is cluttered with sprites which aren't supposed to be visible,
      increasing the coordinate mask in k053247_sprites_draw() from 0x3ff to 0xfff
      fixes this but breaks other games (e.g. Vendetta).

***************************************************************************/

#include "emu.h"

#include "machine/k053252.h"
#include "machine/timer.h"
#include "machine/watchdog.h"
#include "k053246_k053247_k055673.h"
#include "k053250.h"
#include "k053251.h"

#include "cpu/m68000/m68000.h"
#include "cpu/m6809/m6809.h"
#include "machine/adc0804.h"
#include "machine/eepromser.h"
#include "machine/rescap.h"
#include "sound/k053260.h"
#include "sound/ymopm.h"
#include "video/k051316.h"

#include "emupal.h"
#include "screen.h"
#include "speaker.h"

#include "overdriv.lh"

#include <algorithm>

namespace {

class overdriv_state : public driver_device
{
public:
	overdriv_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_maincpu(*this, "maincpu")
		, m_subcpu(*this, "sub")
		, m_audiocpu(*this, "audiocpu")
		, m_k051316(*this, "k051316_%u", 1)
		, m_k053250(*this, "k053250_%u", 1)
		, m_k053246(*this, "k053246")
		, m_k053251(*this, "k053251")
		, m_k053252(*this, "k053252")
		, m_watchdog(*this, "watchdog")
		, m_screen(*this, "screen")
		, m_share1(*this, "share1")
		, m_roadram(*this, "roadram")
		, m_led(*this, "led0")
	{ }

	void overdriv(machine_config &config);

protected:
	virtual void machine_start() override ATTR_COLD;
	virtual void machine_reset() override ATTR_COLD;

private:
	void eeprom_w(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);
	void cpuA_ctrl_w(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);
	uint16_t cpuB_ctrl_r();
	void cpuB_ctrl_w(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);
	void soundirq_w(uint16_t data);
	void sound_ack_w(uint8_t data);
	void sub_irq4_assert_w(uint16_t data);
	void sub_irq5_assert_w(uint16_t data);
	void objdma_w(uint8_t data);
	void io_latch_w(uint8_t data);
	uint8_t io_status_r();
	void io_ack_w(uint8_t data);
	void sub_alu_w(uint8_t data);
	TIMER_CALLBACK_MEMBER(objdma_end_cb);

	uint32_t screen_update(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);
	[[maybe_unused]] INTERRUPT_GEN_MEMBER(cpuB_interrupt);
	TIMER_DEVICE_CALLBACK_MEMBER(cpuA_scanline);

	K051316_CB_MEMBER(zoom_callback_1);
	K051316_CB_MEMBER(zoom_callback_2);
	K053246_CB_MEMBER(sprite_callback);
	void main_map(address_map &map) ATTR_COLD;
	void sound_map(address_map &map) ATTR_COLD;
	void sub_map(address_map &map) ATTR_COLD;

	/* video-related */
	uint16_t  m_zoom_colorbase[2]{};
	uint16_t  m_road_colorbase[2]{};
	uint16_t  m_sprite_colorbase = 0;
	int32_t   m_layerpri[4]{};
	int       m_sprite_pass = 0;
	emu_timer *m_objdma_end_timer = nullptr;

	/* misc */
	uint16_t  m_cpuB_ctrl = 0;
	uint8_t   m_io_latch = 0;

	/* devices */
	required_device<cpu_device> m_maincpu;
	required_device<cpu_device> m_subcpu;
	required_device<cpu_device> m_audiocpu;
	required_device_array<k051316_device, 2> m_k051316;
	required_device_array<k053250_device, 2> m_k053250;
	required_device<k053247_device> m_k053246;
	required_device<k053251_device> m_k053251;
	required_device<k053252_device> m_k053252;
	required_device<watchdog_timer_device> m_watchdog;
	required_device<screen_device> m_screen;
	required_shared_ptr<uint16_t> m_share1;
	required_shared_ptr<uint16_t> m_roadram;
	output_finder<> m_led;
};


/***************************************************************************

  EEPROM

***************************************************************************/

static const uint16_t overdriv_default_eeprom[64] =
{
	0x7758,0xFFFF,0x0078,0x9000,0x0078,0x7000,0x0078,0x5000,
	0x5441,0x4B51,0x3136,0x4655,0x4AFF,0x0300,0x0270,0x0250,
	0x00B4,0x0300,0xB403,0x00B4,0x0300,0xB403,0x00B4,0x0300,
	0xB403,0x00B4,0x0300,0xB403,0x00B4,0x0300,0xB403,0x00B4,
	0x0300,0xB403,0x00B4,0x0300,0xB403,0x00B4,0x0300,0xB403,
	0x00B4,0x0300,0xB403,0x00B4,0x0300,0xB403,0x00B4,0x0300,
	0xB403,0x00B4,0x0300,0xB403,0x00B4,0x0300,0xB403,0x00B4,
	0x0300,0xB403,0x00B4,0x0300,0xB403,0x00B4,0x0300,0xB403
};


void overdriv_state::eeprom_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	//logerror("%s: write %04x to eeprom_w\n",machine().describe_context(),data);
	if (ACCESSING_BITS_0_7)
	{
		/* bit 0 is data */
		/* bit 1 is clock (active high) */
		/* bit 2 is cs (active low) */
		ioport("EEPROMOUT")->write(data, 0xff);
	}
}

TIMER_DEVICE_CALLBACK_MEMBER(overdriv_state::cpuA_scanline)
{
	int scanline = param;

	// IRQ4: vblank-out. Firmware zeros $400B2 and kicks the watchdog here.
	if (scanline == 256)
	{
		m_maincpu->set_input_line(4, HOLD_LINE);
		return;
	}

	// $200001 phase / $200003 attract (low bytes of $200000/$200002).
	// Demo play (phase 1 index 1) and driving (phase >= 2) need a second IRQ5
	// late in the frame so $400B2 reaches 3 and the chip0 ROZ MOVEP runs.
	// Title (phase 1 index >= 2) and boot stay at one IRQ5 per frame.
	// Lines 0/140 were picked against the PCB recording: TIME counts down
	// 0.680/s (PCB 0.675/s) and the chip0 MOVEP still runs 0.49x per frame.
	// The old 0/56/112/168 schedule ran the game logic 1.4x too fast
	// (0.963/s) for the same MOVEP rate.
	const uint8_t phase = uint8_t(m_share1[0]);
	const uint8_t attract = uint8_t(m_share1[1]);
	const bool play_irq5 = (phase >= 2) || (phase == 1 && attract == 1);
	const bool irq5 = play_irq5
		? (scanline == 0 || scanline == 140)
		: (scanline == 0);

	if (irq5)
	{
		// HOLD_LINE stacks if the previous IRQ5 is still running (IPL=5).
		const int ipl = (int(m_maincpu->state_int(M68K_SR)) >> 8) & 7;
		if (ipl < 5)
			m_maincpu->set_input_line(5, HOLD_LINE);
	}
}

INTERRUPT_GEN_MEMBER(overdriv_state::cpuB_interrupt)
{
	// this doesn't get turned on until the irq has happened? wrong irq?
}

void overdriv_state::cpuA_ctrl_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	if (ACCESSING_BITS_0_7)
	{
		/* bit 0 probably enables the second 68000 */
		m_subcpu->set_input_line(INPUT_LINE_RESET, (data & 0x01) ? CLEAR_LINE : ASSERT_LINE);

		/* bit 1 is clear during service mode - function unknown */

		m_led = BIT(data, 3);
		machine().bookkeeping().coin_counter_w(0, data & 0x10);
		machine().bookkeeping().coin_counter_w(1, data & 0x20);

		//logerror("%s: write %04x to cpuA_ctrl_w\n",machine().describe_context(),data);
	}
}

uint16_t overdriv_state::cpuB_ctrl_r()
{
	return m_cpuB_ctrl;
}

void overdriv_state::cpuB_ctrl_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{
	COMBINE_DATA(&m_cpuB_ctrl);

	if (ACCESSING_BITS_0_7)
	{
		/* bit 0 = OBJCHA / sprite ROM window ($128001) */
		m_k053246->k053246_set_objcha_line( (data & 0x01) ? ASSERT_LINE : CLEAR_LINE);

		/* bit 1 set by sub TRAP #0 ($104C = #2); not an IRQ mask */
	}
}

void overdriv_state::io_latch_w(uint8_t data)
{
	m_io_latch = data;
}

uint8_t overdriv_state::io_status_r()
{
	// Firmware waits for == 1 then writes ack $0E0005. PCB chip is unidentified;
	// never-ready (0) matches the previous unmapped read and avoids a false handshake.
	return 0;
}

void overdriv_state::io_ack_w(uint8_t /*data*/)
{
}

void overdriv_state::sub_alu_w(uint8_t data)
{
	// Perspective coprocessor. Mailbox is the top of $208000 RAM.
	// $20BFC8.w / $20BFCA.w -> $20BFCC.l  ($18, 16/16 -> Q16.16)
	// $20BFE4.l / $20BFE8.w -> $20BFEA.l  ($1B, 32/16)
	auto put32 = [this](unsigned word_off, int32_t value)
	{
		m_roadram[word_off]     = uint16_t(uint32_t(value) >> 16);
		m_roadram[word_off + 1] = uint16_t(uint32_t(value));
	};

	switch (data)
	{
	case 0x18:
		{
			int16_t num = int16_t(m_roadram[0x1fe4]); // $20BFC8
			int16_t den = int16_t(m_roadram[0x1fe5]); // $20BFCA
			int32_t q = den ? int32_t((int64_t(num) << 16) / den) : 0;
			put32(0x1fe6, q); // $20BFCC
			break;
		}
	case 0x1b:
		{
			int32_t num = int32_t((uint32_t(m_roadram[0x1ff2]) << 16) | m_roadram[0x1ff3]); // $20BFE4
			int16_t den = int16_t(m_roadram[0x1ff4]); // $20BFE8
			int32_t q = den ? int32_t(int64_t(num) / den) : 0;
			put32(0x1ff5, q); // $20BFEA
			break;
		}
	case 0x1d:
	case 0x1e:
		// Load / start strobes around the $208000 segment buffer. No mailbox write.
		break;
	default:
		break;
	}
}

void overdriv_state::soundirq_w(uint16_t data)
{
	m_audiocpu->set_input_line(M6809_IRQ_LINE, ASSERT_LINE);
}




void overdriv_state::sub_irq4_assert_w(uint16_t data)
{
	// used in-game
	m_subcpu->set_input_line(4, HOLD_LINE);
}

void overdriv_state::sub_irq5_assert_w(uint16_t data)
{
	// tests GFX ROMs with this irq (indeed enabled only in test mode)
	m_subcpu->set_input_line(5, HOLD_LINE);
}


/***************************************************************************

  Callbacks for the K053247

***************************************************************************/

K053246_CB_MEMBER(overdriv_state::sprite_callback)
{
	// word6 bits 5-10: 6-bit 053251 priority, smaller is in front.
	// Priority bitmap holds the per-line LVC priority (0-30, 30 = backdrop).
	// Pass 0 draws pri>=1 behind the dashboard, pass 1 draws pri 0 above it.
	const int pri = (color & 0x7e0) >> 5;
	if (m_sprite_pass == 0)
		priority_mask = pri ? int((1u << std::min(pri, 30)) - 1) : 0x7fffffff;
	else
		priority_mask = pri ? 0x7fffffff : 0;

	color = m_sprite_colorbase + (color & 0x001f);
}


/***************************************************************************

  Callbacks for the K051316

***************************************************************************/

K051316_CB_MEMBER(overdriv_state::zoom_callback_1)
{
	code |= ((color & 0x03) << 8);
	color = m_zoom_colorbase[0] + ((color & 0x3c) >> 2);
}

K051316_CB_MEMBER(overdriv_state::zoom_callback_2)
{
	code |= ((color & 0x03) << 8);
	color = m_zoom_colorbase[1] + ((color & 0x3c) >> 2);
}


/***************************************************************************

  Display refresh

***************************************************************************/

uint32_t overdriv_state::screen_update(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	m_sprite_colorbase  = m_k053251->get_palette_index(k053251_device::CI0);
	m_road_colorbase[1] = m_k053251->get_palette_index(k053251_device::CI1);
	m_road_colorbase[0] = m_k053251->get_palette_index(k053251_device::CI2);

	for (int i = 0; i < 2; i++)
	{
		const int prev_colorbase = m_zoom_colorbase[i];
		m_zoom_colorbase[i] = m_k053251->get_palette_index(k053251_device::CI4 - i);

		if (m_zoom_colorbase[i] != prev_colorbase)
			m_k051316[i]->mark_tmap_dirty();
	}

	// 053251: CI4 skybox reg $3E, CI3 dashboard reg 0, CI0-2 (OBJ, LVC) use pins.
	// Priority bitmap = 6-bit priority clamped to 0-30 (31 is taken by pdrawgfx).
	const int sky_pri = m_k053251->get_priority(k053251_device::CI4);
	const int lvc_flags = k053250_device::DRAW_LINE_PRIORITY | k053250_device::DRAW_NO_LINE_WRAP | k053250_device::DRAW_FLIPX_9BIT;

	screen.priority().fill(0, cliprect);
	m_k051316[0]->zoom_draw(screen, bitmap, cliprect, TILEMAP_DRAW_OPAQUE, 30);          // skybox
	m_k053250[0]->draw(bitmap, cliprect, m_road_colorbase[1], lvc_flags, screen.priority(), sky_pri); // LVC A: CI1
	m_k053250[1]->draw(bitmap, cliprect, m_road_colorbase[0], lvc_flags, screen.priority(), sky_pri); // LVC B: CI2
	m_sprite_pass = 0;
	m_k053246->k053247_sprites_draw(bitmap, cliprect);                                     // world OBJ
	m_k051316[1]->zoom_draw(screen, bitmap, cliprect, 0, 0);                              // dashboard (CI3 = 0)
	screen.priority().fill(0, cliprect);
	m_sprite_pass = 1;
	m_k053246->k053247_sprites_draw(bitmap, cliprect);                                     // pri 0 OBJ (hands, HUD)
	return 0;
}


void overdriv_state::main_map(address_map &map)
{
	map(0x000000, 0x03ffff).rom();
	map(0x040000, 0x043fff).ram();                 /* work RAM */
	map(0x080000, 0x080fff).ram().w("palette", FUNC(palette_device::write16)).share("palette");
	map(0x0c0000, 0x0c0001).portr("INPUTS");
	map(0x0c0002, 0x0c0003).portr("SYSTEM");
	map(0x0e0001, 0x0e0001).w(FUNC(overdriv_state::io_latch_w));
	map(0x0e0003, 0x0e0003).r(FUNC(overdriv_state::io_status_r));
	map(0x0e0005, 0x0e0005).w(FUNC(overdriv_state::io_ack_w));
	map(0x100000, 0x10001f).rw(m_k053252, FUNC(k053252_device::read), FUNC(k053252_device::write)).umask16(0x00ff); /* K053252 CCU (LSB) */
	map(0x140000, 0x140000).w(m_watchdog, FUNC(watchdog_timer_device::reset_w)); /* MOVE.B #1, $140000 */
	map(0x180001, 0x180001).rw("adc", FUNC(adc0804_device::read), FUNC(adc0804_device::write));
	map(0x1c0000, 0x1c001f).w(m_k051316[0], FUNC(k051316_device::ctrl_w)).umask16(0xff00);
	map(0x1c8000, 0x1c801f).w(m_k051316[1], FUNC(k051316_device::ctrl_w)).umask16(0xff00);
	map(0x1d0000, 0x1d001f).w(m_k053251, FUNC(k053251_device::write)).umask16(0xff00);
	map(0x1d8000, 0x1d8003).rw("k053260_1", FUNC(k053260_device::main_read), FUNC(k053260_device::main_write)).umask16(0x00ff);
	map(0x1e0000, 0x1e0003).rw("k053260_2", FUNC(k053260_device::main_read), FUNC(k053260_device::main_write)).umask16(0x00ff);
	map(0x1e8000, 0x1e8001).w(FUNC(overdriv_state::soundirq_w));
	map(0x1f0000, 0x1f0001).w(FUNC(overdriv_state::cpuA_ctrl_w));  /* halt cpu B, coin counter, start lamp, other? */
	map(0x1f8000, 0x1f8001).w(FUNC(overdriv_state::eeprom_w));
	map(0x200000, 0x203fff).ram().share("share1");
	map(0x210000, 0x210fff).rw(m_k051316[0], FUNC(k051316_device::read), FUNC(k051316_device::write)).umask16(0xff00);
	map(0x218000, 0x218fff).rw(m_k051316[1], FUNC(k051316_device::read), FUNC(k051316_device::write)).umask16(0xff00);
	map(0x220000, 0x220fff).r(m_k051316[0], FUNC(k051316_device::rom_r)).umask16(0xff00);
	map(0x228000, 0x228fff).r(m_k051316[1], FUNC(k051316_device::rom_r)).umask16(0xff00);
	map(0x230000, 0x230001).w(FUNC(overdriv_state::sub_irq4_assert_w));
	map(0x238000, 0x238001).w(FUNC(overdriv_state::sub_irq5_assert_w));
}

TIMER_CALLBACK_MEMBER(overdriv_state::objdma_end_cb)
{
	m_subcpu->set_input_line(6, HOLD_LINE);
}

void overdriv_state::objdma_w(uint8_t data)
{
	if(data & 0x10)
		m_objdma_end_timer->adjust(attotime::from_usec(100));

	m_k053246->k053246_w(5, data);
}

void overdriv_state::sub_map(address_map &map)
{
	map(0x000000, 0x03ffff).rom();
	map(0x080000, 0x083fff).ram(); /* work RAM */
	map(0x0c0000, 0x0c0fff).rw(m_k053250[1], FUNC(k053250_device::ram_r), FUNC(k053250_device::ram_w)); // LVC B (road)
	map(0x0c1000, 0x0c1fff).rw(m_k053250[0], FUNC(k053250_device::ram_r), FUNC(k053250_device::ram_w)); // LVC A (bridges/walls)
	map(0x100000, 0x10000f).rw(m_k053250[0], FUNC(k053250_device::reg_r), FUNC(k053250_device::reg_w));
	map(0x108000, 0x10800f).rw(m_k053250[1], FUNC(k053250_device::reg_r), FUNC(k053250_device::reg_w));
	map(0x118000, 0x118fff).rw(m_k053246, FUNC(k053247_device::k053247_word_r), FUNC(k053247_device::k053247_word_w)); // data gets copied to sprite chip with DMA..
	map(0x120000, 0x120001).r(m_k053246, FUNC(k053247_device::k053246_r));
	map(0x128000, 0x128001).rw(FUNC(overdriv_state::cpuB_ctrl_r), FUNC(overdriv_state::cpuB_ctrl_w)); /* enable K053247 ROM reading, plus something else */
	map(0x130000, 0x130007).rw(m_k053246, FUNC(k053247_device::k053246_r), FUNC(k053247_device::k053246_w));
	map(0x130005, 0x130005).w(FUNC(overdriv_state::objdma_w));
	map(0x140001, 0x140001).w(FUNC(overdriv_state::sub_alu_w));
	map(0x200000, 0x203fff).ram().share("share1");
	map(0x208000, 0x20bfff).ram().share("roadram"); // road segments + $20BFxx ALU mailbox
	map(0x218000, 0x219fff).r(m_k053250[0], FUNC(k053250_device::rom_r));
	map(0x220000, 0x221fff).r(m_k053250[1], FUNC(k053250_device::rom_r));
}

void overdriv_state::sound_ack_w(uint8_t data)
{
	m_audiocpu->set_input_line(M6809_IRQ_LINE, CLEAR_LINE);
}

void overdriv_state::sound_map(address_map &map)
{
	map(0x0000, 0x0000).w(FUNC(overdriv_state::sound_ack_w));
	// 0x012 read during explosions
	// 0x180
	map(0x0200, 0x0201).rw("ymsnd", FUNC(ym2151_device::read), FUNC(ym2151_device::write));
	map(0x0400, 0x042f).rw("k053260_1", FUNC(k053260_device::read), FUNC(k053260_device::write));
	map(0x0600, 0x062f).rw("k053260_2", FUNC(k053260_device::read), FUNC(k053260_device::write));
	map(0x0800, 0x0fff).ram();
	map(0x1000, 0xffff).rom();
}

/* Both IPT_START1 assignments are needed. The game will reset during */
/* the "continue" sequence if the assignment on the first port        */
/* is missing.                                                        */

static INPUT_PORTS_START( overdriv )
	PORT_START("INPUTS")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON3 ) PORT_TOGGLE
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_CUSTOM ) PORT_READ_LINE_DEVICE_MEMBER("eeprom", FUNC(eeprom_serial_er5911_device::do_read))
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_CUSTOM ) PORT_READ_LINE_DEVICE_MEMBER("eeprom", FUNC(eeprom_serial_er5911_device::ready_read))

	PORT_START("SYSTEM")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_SERVICE_NO_TOGGLE( 0x10, IP_ACTIVE_LOW )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_CUSTOM ) PORT_READ_LINE_DEVICE_MEMBER("adc", FUNC(adc0804_device::intr_r))

	PORT_START("PADDLE")
	PORT_BIT( 0xff, 0x80, IPT_PADDLE ) PORT_SENSITIVITY(100) PORT_KEYDELTA(50)
	// POST checks if paddle is at center otherwise throws a "VOLUME ERROR"

	PORT_START( "EEPROMOUT" )
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_OUTPUT ) PORT_WRITE_LINE_DEVICE_MEMBER("eeprom", FUNC(eeprom_serial_er5911_device::di_write))
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_OUTPUT ) PORT_WRITE_LINE_DEVICE_MEMBER("eeprom", FUNC(eeprom_serial_er5911_device::clk_write))
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_OUTPUT ) PORT_WRITE_LINE_DEVICE_MEMBER("eeprom", FUNC(eeprom_serial_er5911_device::cs_write))
INPUT_PORTS_END


void overdriv_state::machine_start()
{
	m_objdma_end_timer = timer_alloc(FUNC(overdriv_state::objdma_end_cb), this);

	save_item(NAME(m_cpuB_ctrl));
	save_item(NAME(m_io_latch));
	save_item(NAME(m_sprite_colorbase));
	save_item(NAME(m_zoom_colorbase));
	save_item(NAME(m_road_colorbase));
	save_item(NAME(m_layerpri));
	save_item(NAME(m_sprite_pass));
}

void overdriv_state::machine_reset()
{
	for (int i = 0; i < 4; i++)
		m_layerpri[i] = 0;

	m_cpuB_ctrl = 0;
	m_io_latch = 0x30;
	m_sprite_colorbase = 0;
	m_zoom_colorbase[0] = 0;
	m_zoom_colorbase[1] = 0;
	m_road_colorbase[0] = 0;
	m_road_colorbase[1] = 0;

	/* start with cpu B halted */
	m_subcpu->set_input_line(INPUT_LINE_RESET, ASSERT_LINE);
}


void overdriv_state::overdriv(machine_config &config)
{
	/* basic machine hardware */
	M68000(config, m_maincpu, 24_MHz_XTAL / 2); /* 12 MHz */
	m_maincpu->set_addrmap(AS_PROGRAM, &overdriv_state::main_map);
	TIMER(config, "scantimer").configure_scanline(FUNC(overdriv_state::cpuA_scanline), "screen", 0, 1);

	M68000(config, m_subcpu, 24_MHz_XTAL / 2);  /* 12 MHz */
	m_subcpu->set_addrmap(AS_PROGRAM, &overdriv_state::sub_map);
	// Sub IRQ4: main $230000. IRQ5: main $238000 (GFX ROM check). IRQ6: OBJ DMA end.

	/* 1.789 MHz?? This might be the right speed, but ROM testing */
	/* takes a little too much (the counter wraps from 0000 to 9999). */
	/* This might just mean that the video refresh rate is less than */
	/* 60 fps, that's how I fixed it for now. */
	MC6809E(config, m_audiocpu, 3.579545_MHz_XTAL);
	m_audiocpu->set_addrmap(AS_PROGRAM, &overdriv_state::sound_map);

	config.set_maximum_quantum(attotime::from_hz(12000));

	EEPROM_ER5911_16BIT(config, "eeprom").default_data(overdriv_default_eeprom, 128);

	// Hardware is a short vblank watchdog, but K053252 res_change() calls
	// screen.configure() on every boot MOVEM write and can count those as
	// vblanks. Time-based avoids a reset loop through POST / $1110 delay.
	WATCHDOG_TIMER(config, m_watchdog).set_time(attotime::from_seconds(3));

	ADC0804(config, "adc", RES_K(10), CAP_P(150)).vin_callback().set_ioport("PADDLE");

	/* video hardware */
	screen_device &screen(SCREEN(config, "screen"));
	screen.set_raw(24_MHz_XTAL / 4, 384, 0, 305, 264, 0, 224);
	screen.set_screen_update(FUNC(overdriv_state::screen_update));
	screen.set_palette("palette");

	PALETTE(config, "palette").set_format(palette_device::xBGR_555, 2048).enable_shadows();

	K053246(config, m_k053246, 24_MHz_XTAL);
	m_k053246->set_sprite_callback(FUNC(overdriv_state::sprite_callback));
	m_k053246->set_config(NORMAL_PLANE_ORDER, -45, 38);
	m_k053246->set_palette("palette");

	K051316(config, m_k051316[0], 24_MHz_XTAL / 2);
	m_k051316[0]->set_palette("palette");
	m_k051316[0]->set_offsets(7, -16);
	m_k051316[0]->set_wrap(1);
	m_k051316[0]->set_zoom_callback(FUNC(overdriv_state::zoom_callback_1));

	K051316(config, m_k051316[1], 24_MHz_XTAL / 2);
	m_k051316[1]->set_palette("palette");
	m_k051316[1]->set_offsets(7, -16);
	m_k051316[1]->set_zoom_callback(FUNC(overdriv_state::zoom_callback_2));

	K053251(config, m_k053251);

	// LVC A's x offset is +4 instead of 0 as a stop-gap: its per-line scroll values
	// only carry about 3/4 of the relief the PCB shows (17 rows across the half
	// width against 22 for the road and for the PCB's own railing), so the bridge
	// railing floats up to 6 pixels above the road at the screen edges. +4 buries
	// the over-corrected centre behind the road and leaves at most 2 pixels of gap.
	// The real fix is the $140001 perspective coprocessor, which still has a
	// guessed mailbox layout (see sub_alu_w).
	K053250(config, m_k053250[0], "palette", m_screen, 4, -16); // LVC A: $100000 / $C1000 / e18-e20
	K053250(config, m_k053250[1], "palette", m_screen, 0, -16); // LVC B: $108000 / $C0000 / e17-e16

	K053252(config, m_k053252, 24_MHz_XTAL / 4);
	// Boot image HC=384 VC=264 vis 305x224. offsets(104,16) pushed max_x to 408 (past HTOTAL).
	m_k053252->set_offsets(0, 0);

	/* sound hardware */
	SPEAKER(config, "speaker", 2).front();

	ym2151_device &ymsnd(YM2151(config, "ymsnd", 3.579545_MHz_XTAL));
	ymsnd.add_route(0, "speaker", 0.5, 0);
	ymsnd.add_route(1, "speaker", 0.5, 1);

	k053260_device &k053260_1(K053260(config, "k053260_1", 3.579545_MHz_XTAL));
	k053260_1.set_device_rom_tag("k053260");
	k053260_1.add_route(0, "speaker", 0.35, 0);
	k053260_1.add_route(1, "speaker", 0.35, 1);

	k053260_device &k053260_2(K053260(config, "k053260_2", 3.579545_MHz_XTAL));
	k053260_2.set_device_rom_tag("k053260");
	k053260_2.add_route(0, "speaker", 0.35, 0);
	k053260_2.add_route(1, "speaker", 0.35, 1);
}



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( overdriv )
	ROM_REGION( 0x40000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "789_n05.d17", 0x00000, 0x20000, CRC(f7885713) SHA1(8e84929dcc6ab889c3e11c450d22c56b183b0198) )
	ROM_LOAD16_BYTE( "789_n04.b17", 0x00001, 0x20000, CRC(aefe87a6) SHA1(1bdf5a1f4c5e2b84d02b2981b3be91ed2406a1f8) )

	ROM_REGION( 0x40000, "sub", 0 )
	ROM_LOAD16_BYTE( "789_e09.l10", 0x00000, 0x20000, CRC(46fb7e88) SHA1(f706a76aff9bec64abe6da325cba0715d6e6ed0a) ) /* also found labeled as "4" as well as "7" */
	ROM_LOAD16_BYTE( "789_e08.k10", 0x00001, 0x20000, CRC(24427195) SHA1(48f4f81729acc0e497b40fddbde11242c5c4c573) ) /* also found labeled as "3" as well as "6" */

	ROM_REGION( 0x10000, "audiocpu", 0 )
	ROM_LOAD( "789_e01.e4", 0x00000, 0x10000, CRC(1085f069) SHA1(27228cedb357ff2e130a4bd6d8aa01cf537e034f) ) /* also found labeled as "5" */

	ROM_REGION( 0x400000, "k053246", 0 )   /* graphics (addressable by the CPU) */
	ROM_LOAD64_WORD( "789e12.r1",  0x000000, 0x100000, CRC(14a10fb2) SHA1(03fb9c15514c5ecc2d9ae4a53961c4bbb49cec73) )    /* sprites */
	ROM_LOAD64_WORD( "789e13.r4",  0x000002, 0x100000, CRC(6314a628) SHA1(f8a8918998c266109348c77427a7696b503daeb3) )
	ROM_LOAD64_WORD( "789e14.r10", 0x000004, 0x100000, CRC(b5eca14b) SHA1(a1c5f5e9cd8bbcfc875e2acb33be024724da63aa) )
	ROM_LOAD64_WORD( "789e15.r15", 0x000006, 0x100000, CRC(5d93e0c3) SHA1(d5cb7666c0c28fd465c860c7f9dbb18a7f739a93) )

	ROM_REGION( 0x020000, "k051316_1", 0 )
	ROM_LOAD( "789e06.a21", 0x000000, 0x020000, CRC(14a085e6) SHA1(86dad6f223e13ff8af7075c3d99bb0a83784c384) )    /* zoom/rotate */

	ROM_REGION( 0x020000, "k051316_2", 0 )
	ROM_LOAD( "789e07.c23", 0x000000, 0x020000, CRC(8a6ceab9) SHA1(1a52b7361f71a6126cd648a76af00223d5b25c7a) )    /* zoom/rotate */

	ROM_REGION( 0x0c0000, "k053250_1", 0 )
	ROM_LOAD( "789e18.p22", 0x000000, 0x040000, CRC(985a4a75) SHA1(b726166c295be6fbec38a9d11098cc4a4a5de456) )
	ROM_LOAD( "789e19.r22", 0x040000, 0x040000, CRC(15c54ea2) SHA1(5b10bd28e48e51613359820ba8c75d4a91c2d322) )
	ROM_LOAD( "789e20.s22", 0x080000, 0x040000, CRC(ea204acd) SHA1(52b8c30234eaefcba1074496028a4ac2bca48e95) )

	ROM_REGION( 0x080000, "k053250_2", 0 )
	ROM_LOAD( "789e17.p17", 0x000000, 0x040000, CRC(04c07248) SHA1(873445002cbf90c9fc5a35bf4a8f6c43193ee342) )
	ROM_LOAD( "789e16.p12", 0x040000, 0x040000, CRC(9348dee1) SHA1(367193373e28962b5b0e54cc15d68ed88ab83f12) )

	ROM_REGION( 0x200000, "k053260", 0 ) /* 053260 samples */
	ROM_LOAD( "789e03.j1", 0x000000, 0x100000, CRC(51ebfebe) SHA1(17f0c23189258e801f48d5833fe934e7a48d071b) )
	ROM_LOAD( "789e02.f1", 0x100000, 0x100000, CRC(bdd3b5c6) SHA1(412332d64052c0a3714f4002c944b0e7d32980a4) )
ROM_END

ROM_START( overdriva )
	ROM_REGION( 0x40000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "2.d17", 0x00000, 0x20000, CRC(77f18f3f) SHA1(a8c91435573c7851a7864d07eeacfb2f142abbe2) )
	ROM_LOAD16_BYTE( "1.b17", 0x00001, 0x20000, CRC(4f44e6ad) SHA1(9fa871f55e6b2ec353dd979ded568cd9da83f5d6) ) /* also found labeled as "3" */

	ROM_REGION( 0x40000, "sub", 0 )
	ROM_LOAD16_BYTE( "789_e09.l10", 0x00000, 0x20000, CRC(46fb7e88) SHA1(f706a76aff9bec64abe6da325cba0715d6e6ed0a) ) /* also found labeled as "4" as well as "7" */
	ROM_LOAD16_BYTE( "789_e08.k10", 0x00001, 0x20000, CRC(24427195) SHA1(48f4f81729acc0e497b40fddbde11242c5c4c573) ) /* also found labeled as "3" as well as "6" */

	ROM_REGION( 0x10000, "audiocpu", 0 )
	ROM_LOAD( "789_e01.e4", 0x00000, 0x10000, CRC(1085f069) SHA1(27228cedb357ff2e130a4bd6d8aa01cf537e034f) ) /* also found labeled as "5" */

	ROM_REGION( 0x400000, "k053246", 0 )   /* graphics (addressable by the CPU) */
	ROM_LOAD64_WORD( "789e12.r1",  0x000000, 0x100000, CRC(14a10fb2) SHA1(03fb9c15514c5ecc2d9ae4a53961c4bbb49cec73) )    /* sprites */
	ROM_LOAD64_WORD( "789e13.r4",  0x000002, 0x100000, CRC(6314a628) SHA1(f8a8918998c266109348c77427a7696b503daeb3) )
	ROM_LOAD64_WORD( "789e14.r10", 0x000004, 0x100000, CRC(b5eca14b) SHA1(a1c5f5e9cd8bbcfc875e2acb33be024724da63aa) )
	ROM_LOAD64_WORD( "789e15.r15", 0x000006, 0x100000, CRC(5d93e0c3) SHA1(d5cb7666c0c28fd465c860c7f9dbb18a7f739a93) )

	ROM_REGION( 0x020000, "k051316_1", 0 )
	ROM_LOAD( "789e06.a21", 0x000000, 0x020000, CRC(14a085e6) SHA1(86dad6f223e13ff8af7075c3d99bb0a83784c384) )    /* zoom/rotate */

	ROM_REGION( 0x020000, "k051316_2", 0 )
	ROM_LOAD( "789e07.c23", 0x000000, 0x020000, CRC(8a6ceab9) SHA1(1a52b7361f71a6126cd648a76af00223d5b25c7a) )    /* zoom/rotate */

	ROM_REGION( 0x0c0000, "k053250_1", 0 )
	ROM_LOAD( "789e18.p22", 0x000000, 0x040000, CRC(985a4a75) SHA1(b726166c295be6fbec38a9d11098cc4a4a5de456) )
	ROM_LOAD( "789e19.r22", 0x040000, 0x040000, CRC(15c54ea2) SHA1(5b10bd28e48e51613359820ba8c75d4a91c2d322) )
	ROM_LOAD( "789e20.s22", 0x080000, 0x040000, CRC(ea204acd) SHA1(52b8c30234eaefcba1074496028a4ac2bca48e95) )

	ROM_REGION( 0x080000, "k053250_2", 0 )
	ROM_LOAD( "789e17.p17", 0x000000, 0x040000, CRC(04c07248) SHA1(873445002cbf90c9fc5a35bf4a8f6c43193ee342) )
	ROM_LOAD( "789e16.p12", 0x040000, 0x040000, CRC(9348dee1) SHA1(367193373e28962b5b0e54cc15d68ed88ab83f12) )

	ROM_REGION( 0x200000, "k053260", 0 ) /* 053260 samples */
	ROM_LOAD( "789e03.j1", 0x000000, 0x100000, CRC(51ebfebe) SHA1(17f0c23189258e801f48d5833fe934e7a48d071b) )
	ROM_LOAD( "789e02.f1", 0x100000, 0x100000, CRC(bdd3b5c6) SHA1(412332d64052c0a3714f4002c944b0e7d32980a4) )
ROM_END

ROM_START( overdrivb )
	ROM_REGION( 0x40000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "4.d17", 0x00000, 0x20000, CRC(93c8e892) SHA1(fb41bb13787b93f533b962c3119e6b9f61e2f3f3) )
	ROM_LOAD16_BYTE( "3.b17", 0x00001, 0x20000, CRC(4f44e6ad) SHA1(9fa871f55e6b2ec353dd979ded568cd9da83f5d6) ) /* also found labeled as "1" */

	ROM_REGION( 0x40000, "sub", 0 )
	ROM_LOAD16_BYTE( "789_e09.l10", 0x00000, 0x20000, CRC(46fb7e88) SHA1(f706a76aff9bec64abe6da325cba0715d6e6ed0a) ) /* also found labeled as "4" as well as "7" */
	ROM_LOAD16_BYTE( "789_e08.k10", 0x00001, 0x20000, CRC(24427195) SHA1(48f4f81729acc0e497b40fddbde11242c5c4c573) ) /* also found labeled as "3" as well as "6" */

	ROM_REGION( 0x10000, "audiocpu", 0 )
	ROM_LOAD( "789_e01.e4", 0x00000, 0x10000, CRC(1085f069) SHA1(27228cedb357ff2e130a4bd6d8aa01cf537e034f) ) /* also found labeled as "5" */

	ROM_REGION( 0x400000, "k053246", 0 )   /* graphics (addressable by the CPU) */
	ROM_LOAD64_WORD( "789e12.r1",  0x000000, 0x100000, CRC(14a10fb2) SHA1(03fb9c15514c5ecc2d9ae4a53961c4bbb49cec73) )    /* sprites */
	ROM_LOAD64_WORD( "789e13.r4",  0x000002, 0x100000, CRC(6314a628) SHA1(f8a8918998c266109348c77427a7696b503daeb3) )
	ROM_LOAD64_WORD( "789e14.r10", 0x000004, 0x100000, CRC(b5eca14b) SHA1(a1c5f5e9cd8bbcfc875e2acb33be024724da63aa) )
	ROM_LOAD64_WORD( "789e15.r15", 0x000006, 0x100000, CRC(5d93e0c3) SHA1(d5cb7666c0c28fd465c860c7f9dbb18a7f739a93) )

	ROM_REGION( 0x020000, "k051316_1", 0 )
	ROM_LOAD( "789e06.a21", 0x000000, 0x020000, CRC(14a085e6) SHA1(86dad6f223e13ff8af7075c3d99bb0a83784c384) )    /* zoom/rotate */

	ROM_REGION( 0x020000, "k051316_2", 0 )
	ROM_LOAD( "789e07.c23", 0x000000, 0x020000, CRC(8a6ceab9) SHA1(1a52b7361f71a6126cd648a76af00223d5b25c7a) )    /* zoom/rotate */

	ROM_REGION( 0x0c0000, "k053250_1", 0 )
	ROM_LOAD( "789e18.p22", 0x000000, 0x040000, CRC(985a4a75) SHA1(b726166c295be6fbec38a9d11098cc4a4a5de456) )
	ROM_LOAD( "789e19.r22", 0x040000, 0x040000, CRC(15c54ea2) SHA1(5b10bd28e48e51613359820ba8c75d4a91c2d322) )
	ROM_LOAD( "789e20.s22", 0x080000, 0x040000, CRC(ea204acd) SHA1(52b8c30234eaefcba1074496028a4ac2bca48e95) )

	ROM_REGION( 0x080000, "k053250_2", 0 )
	ROM_LOAD( "789e17.p17", 0x000000, 0x040000, CRC(04c07248) SHA1(873445002cbf90c9fc5a35bf4a8f6c43193ee342) )
	ROM_LOAD( "789e16.p12", 0x040000, 0x040000, CRC(9348dee1) SHA1(367193373e28962b5b0e54cc15d68ed88ab83f12) )

	ROM_REGION( 0x200000, "k053260", 0 ) /* 053260 samples */
	ROM_LOAD( "789e03.j1", 0x000000, 0x100000, CRC(51ebfebe) SHA1(17f0c23189258e801f48d5833fe934e7a48d071b) )
	ROM_LOAD( "789e02.f1", 0x100000, 0x100000, CRC(bdd3b5c6) SHA1(412332d64052c0a3714f4002c944b0e7d32980a4) )
ROM_END

} // anonymous namespace


GAMEL( 1990, overdriv,         0, overdriv, overdriv, overdriv_state, empty_init, ROT90, "Konami", "Over Drive (set 1)", MACHINE_IMPERFECT_GRAPHICS | MACHINE_NOT_WORKING | MACHINE_SUPPORTS_SAVE, layout_overdriv ) // US version
GAMEL( 1990, overdriva, overdriv, overdriv, overdriv, overdriv_state, empty_init, ROT90, "Konami", "Over Drive (set 2)", MACHINE_IMPERFECT_GRAPHICS | MACHINE_NOT_WORKING | MACHINE_SUPPORTS_SAVE, layout_overdriv ) // Overseas?
GAMEL( 1990, overdrivb, overdriv, overdriv, overdriv, overdriv_state, empty_init, ROT90, "Konami", "Over Drive (set 3)", MACHINE_IMPERFECT_GRAPHICS | MACHINE_NOT_WORKING | MACHINE_SUPPORTS_SAVE, layout_overdriv ) // Overseas?
