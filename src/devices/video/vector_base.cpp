// license:BSD-3-Clause
// copyright-holders:Mario Montminy, Anthony Campbell
#include "emu.h"
#include "emuopts.h"
#include "video/vector_base.h"

#define VERBOSE 0
#include "logmacro.h"


vector_device_base::vector_device_base(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock) 
   : device_t(mconfig, type, tag, owner, clock)
{

									}

int vector_device_base::add_point(int x, int y, rgb_t color, int intensity) 
{
    return 0;
};

int vector_device_base::update(screen_device &screen, const rectangle &cliprect) 
{ 
    return 0; 
};

