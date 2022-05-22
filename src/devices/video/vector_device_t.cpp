// license:BSD-3-Clause
// copyright-holders:Mario Montminy, Anthony Campbell
#include "emu.h"
#include "emuopts.h"
#include "video/vector_device_t.h"

#define VERBOSE 0
#include "logmacro.h"

vector_device_t::vector_device_t(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock)
    : device_t(mconfig, type, tag, owner, clock){};
void vector_device_t::add_point(int x, int y, rgb_t color, int intensity){};
void vector_device_t::device_add_mconfig(machine_config &config){};
void vector_device_t::device_start(){};
uint32_t vector_device_t::screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
    return 0;
};
