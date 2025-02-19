// license:BSD-3-Clause
// copyright-holders:Phil Stroffolino
/***************************************************************************

    Break Thru

***************************************************************************/
#ifndef MAME_INCLUDES_BRKTRHU_H
#define MAME_INCLUDES_BRKTRHU_H

#pragma once

#include "machine/gen_latch.h"
#include "emupal.h"
#include "tilemap.h"

class brkthru_state : public driver_device
{
public:
	brkthru_state(const machine_config &mconfig, device_type type, const char *tag) :
		driver_device(mconfig, type, tag),
		m_fg_videoram(*this, "fg_videoram"),
		m_videoram(*this, "videoram"),
		m_spriteram(*this, "spriteram"),
		m_maincpu(*this, "maincpu"),
		m_audiocpu(*this, "audiocpu"),
		m_gfxdecode(*this, "gfxdecode"),
		m_palette(*this, "palette"),
		m_soundlatch(*this, "soundlatch")
	{ }

	void brkthru(machine_config &config);
	void darwin(machine_config &config);
	DECLARE_INPUT_CHANGED_MEMBER(coin_inserted);
	void init_brkthru();

private:
	/* memory pointers */
	required_shared_ptr<uint8_t> m_fg_videoram;
	required_shared_ptr<uint8_t> m_videoram;
	required_shared_ptr<uint8_t> m_spriteram;

	/* video-related */
	tilemap_t *m_fg_tilemap = nullptr;
	tilemap_t *m_bg_tilemap = nullptr;
	int     m_bgscroll = 0;
	int     m_bgbasecolor = 0;
	int     m_flipscreen = 0;

	/* devices */
	required_device<cpu_device> m_maincpu;
	required_device<cpu_device> m_audiocpu;
	required_device<gfxdecode_device> m_gfxdecode;
	required_device<palette_device> m_palette;
	required_device<generic_latch_8_device> m_soundlatch;

	uint8_t   m_nmi_mask = 0U;
	void brkthru_1803_w(uint8_t data);
	void darwin_0803_w(uint8_t data);
	void brkthru_bgram_w(offs_t offset, uint8_t data);
	void brkthru_fgram_w(offs_t offset, uint8_t data);
	void brkthru_1800_w(offs_t offset, uint8_t data);
	TILE_GET_INFO_MEMBER(get_bg_tile_info);
	TILE_GET_INFO_MEMBER(get_fg_tile_info);
	virtual void machine_start() override;
	virtual void machine_reset() override;
	virtual void video_start() override;
	void brkthru_palette(palette_device &palette) const;
	uint32_t screen_update_brkthru(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);
	DECLARE_WRITE_LINE_MEMBER(vblank_irq);
	void draw_sprites( bitmap_ind16 &bitmap, const rectangle &cliprect, int prio );
	void brkthru_map(address_map &map);
	void darwin_map(address_map &map);
	void sound_map(address_map &map);
};

#endif // MAME_INCLUDES_BRKTRHU_H
