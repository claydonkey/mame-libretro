// license:BSD-3-Clause
// copyright-holders: Anthony Campbell, TH
#include "emu.h"
#include "emuopts.h"
#include "rendutil.h"
#include "video/vector_base.h"
#include "video/vector_v_st.h"               

#include <stdint.h>
#define VERBOSE 0
#define MAX_POINTS 20000

#include "logmacro.h"

DEFINE_DEVICE_TYPE(VECTOR_V_ST, vector_device_v_st, "vector_device_v_st", "VECTOR_V_ST")

int vector_device_v_st::serial_read(uint8_t *buf, int size)
{
    int result = size;
    uint32_t read = 0;

    m_serial->read(buf, 0, size, read);
    if (read != size)
    {
        result = -1;
    }
    return result;
}


int vector_device_v_st::serial_write(uint8_t *buf, int size)
{
    int result = -1;
    uint32_t written = 0, chunk, total;

    total = size;
    while (size)
    {
        chunk = std::min(size, 512);
        m_serial->write(buf, 0, chunk, written);
        if (written != chunk)
        {
            goto END;
        }
        buf  += chunk;
        size -= chunk;
    }
    result = total;
END:
    return result;
}
void vector_device_v_st::serial_reset()
{
	m_serial_offset = 0;
	m_serial_buf[m_serial_offset++] = 0;
	m_serial_buf[m_serial_offset++] = 0;
	m_serial_buf[m_serial_offset++] = 0;
	m_serial_buf[m_serial_offset++] = 0;

	m_vector_transit[0] = 0;
	m_vector_transit[1] = 0;
	m_vector_transit[2] = 0;
}

 

int vector_device_v_st::serial_send()
{
    int result = -1;
	if (m_serial < 0)
		return;

	// add the "done" command to the message
	m_serial_buf[m_serial_offset++] = 1;
	m_serial_buf[m_serial_offset++] = 1;
	m_serial_buf[m_serial_offset++] = 1;

	size_t offset = 0;

	printf("%zu vectors: off=%zu on=%zu bright=%zu%s\n",
		m_serial_offset/3,
		m_vector_transit[0],
		m_vector_transit[1],
		m_vector_transit[2],
		m_serial_drop_frame ? " !" : ""
	);

    result  = serial_write(&m_serial_buf[0], m_serial_offset);
	serial_reset();

    return result;
}

int vector_device_v_st::add_point(int x, int y, rgb_t color, int intensity)
{
	// always flip the Y, since the vectorscope measures
	// 0,0 at the bottom left corner, but this coord uses
	// the top left corner.
	y = 2047 - y;

	// make sure that we are in range; should always be
	// due to clipping on the window, but just in case
	if (x < 0) x = 0;
	if (y < 0) y = 0;

	if (x > 2047) x = 2047;
	if (y > 2047) y = 2047;

	unsigned bright;
	if (intensity > m_serial_bright)
		bright = 3;
	else
	if (intensity > 0)
		bright = 2;
	else
		bright = 1;

	if (m_serial_rotate == 1)
	{
		// +90
		unsigned tmp = x;
		x = 2047 - y;
		y = tmp;
	} else
	if (m_serial_rotate == 2)
	{
		// +180
		x = 2047 - x;
		y = 2047 - y;
	} else
	if (m_serial_rotate == 3)
	{
		// -90
		unsigned t = x;
		x = y;
		y = 2047 - t;
	}

	uint32_t cmd = 0
		| (bright << 22)
		| (x & 0x7FF) << 11
		| (y & 0x7FF) <<  0
		;

	m_serial_buf[m_serial_offset++] = cmd >> 16;
	m_serial_buf[m_serial_offset++] = cmd >>  8;
	m_serial_buf[m_serial_offset++] = cmd >>  0;

	// todo: check for overflow;
	// should always have enough points
}

void vector_device_v_st::serial_draw_line(float xf0,float yf0,float xf1,float yf1,int intensity)
{
	if (m_serial_fd < 0)
		return;

    rgb_t color = rgb_t(255, 255, 255);
	static int last_x;
	static int last_y;

	const int x0 = (xf0 * 2048 - 1024) * m_serial_scale + 1024;
	const int y0 = (yf0 * 2048 - 1024) * m_serial_scale + 1024;
	const int x1 = (xf1 * 2048 - 1024) * m_serial_scale + 1024;
	const int y1 = (yf1 * 2048 - 1024) * m_serial_scale + 1024;

	// if this is not a continuous segment,
	// we must add a transit command
	if (last_x != x0 || last_y != y0)
	{
		add_point(x0, y0, 0, 0);
		int dx = x0 - last_x;
		int dy = y0 - last_y;
		m_vector_transit[0] += sqrt(dx*dx + dy*dy);
	}

	// transit to the new point
	int dx = x1 - x0;
	int dy = y1 - y0;
	int dist = sqrt(dx*dx + dy*dy);

	add_point(x1, y1, color, intensity);
	if (intensity > m_serial_bright)
		m_vector_transit[2] += dist;
	else
		m_vector_transit[1] += dist;

	last_x = x1;
	last_y = y1;
}
vector_device_v_st::vector_device_v_st(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
: vector_device_base(mconfig, VECTOR_V_ST, tag, owner, clock)
{
}

void vector_device_v_st::device_start()
{   
    int i;
    uint64_t size = 0;
    m_mirror = machine().config().options().vector_screen_mirror(); 
    std::error_condition filerr = osd_file::open(machine().config().options().vector_port(), OPEN_FLAG_READ | OPEN_FLAG_WRITE, m_serial, size);
 
	/* Setup the serial output of the XY coords if configured */

	m_serial_scale = machine().options().vector_scale();
	m_serial_rotate = machine().options().vector_rotate();
	m_serial_bright = machine().options().vector_bright();
	m_serial_drop_frame = 0;

	// allocate enough buffer space, although we should never use this much
	m_serial_buf = make_unique_clear<uint8_t[]>((MAX_POINTS+2) * 3);
	if (!m_serial_buf)
	{
		// todo: how to signal an error?
	}

	serial_reset();

}
void vector_device_v_st::device_stop()
{
    serial_write(&m_serial_buf[0], m_serial_offset);
}
void vector_device_v_st::device_reset()
{
}
 
 
int vector_device_v_st::update(screen_device &screen, const rectangle &cliprect)
{
 
    serial_send();
    return 0;
}

