// license:BSD-3-Clause
// copyright-holders: Anthony Campbell, TH
#include "emu.h"
#include "emuopts.h"
#include "rendutil.h"
#include "video/vector.h"
#include "video/vector_v_st.h"

#include <stdint.h>
#define VERBOSE 0
#define MAX_POINTS 20000
#define VECTOR_SERIAL_MAX 4095

#include "logmacro.h"

DEFINE_DEVICE_TYPE(VECTOR_V_ST, vector_device_v_st, "vector_v_st", "VECTOR_V_ST")

int vector_device_v_st::serial_read(uint8_t *buf, int size)
{
    return 0;
}

int vector_device_v_st::serial_write(uint8_t *buf, int size)
{
    return 0;
}
void vector_device_v_st::serial_reset()
{
}

int vector_device_v_st::serial_send()
{
    return 0;
}

void vector_device_v_st::add_point(int x, int y, rgb_t color, int intensity)
{
}

void vector_device_v_st::serial_draw_line(float xf0, float yf0, float xf1, float yf1, int intensity)
{
}
vector_device_v_st::vector_device_v_st(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
    : vector_device_t(mconfig, VECTOR_V_ST, tag, owner, clock)
{
}

void vector_device_v_st::device_start()
{
    vector_device_t::start();
    // allocate enough buffer space, although we should never use this much
    // m_serial_buf = make_unique_clear<uint8_t[]>((MAX_POINTS + 2) * 4);
}
void vector_device_v_st::device_stop()
{

}

void vector_device_v_st::device_reset()
{

}


uint32_t vector_device_v_st::screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
    return 0;
}
