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
#define VECTOR_SERIAL_MAX 4095

#include "logmacro.h"

DEFINE_DEVICE_TYPE(VECTOR_V_ST, vector_device_v_st, "vector_v_st", "VECTOR_V_ST")

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
		return 0;

	// add the "done" command to the message
	m_serial_buf[m_serial_offset++] = 1;
	m_serial_buf[m_serial_offset++] = 1;
	m_serial_buf[m_serial_offset++] = 1;
    m_serial_buf[m_serial_offset++] = 1;

	printf("%zu vectors: off=%zu on=%zu bright=%zu%s\n",
		m_serial_offset/4,
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
 
	// make sure that we are in range; should always be
	// due to clipping on the window, but just in case
	if (x < 0) x = 0;
	if (y < 0) y = 0;

	if (x > VECTOR_SERIAL_MAX) x = VECTOR_SERIAL_MAX;
	if (y > VECTOR_SERIAL_MAX) y = VECTOR_SERIAL_MAX;

	// always flip the Y, since the vectorscope measures
	// 0,0 at the bottom left corner, but this coord uses
	// the top left corner.
	y = VECTOR_SERIAL_MAX - y;

	unsigned bright;
	if (intensity > m_serial_bright)
		bright = 3;
	else
    if (intensity <= 0)
		bright = 0;
	else
		bright = (intensity * 64) / 256;

	if (bright > 63)
		bright = 63;

	if (m_serial_rotate == 1)
	{
		// +90
		unsigned tmp = x;
        x = VECTOR_SERIAL_MAX - y;
		y = tmp;
	} else
	if (m_serial_rotate == 2)
	{
		// +180
		x = VECTOR_SERIAL_MAX - x;
		y = VECTOR_SERIAL_MAX - y;
	} else
	if (m_serial_rotate == 3)
	{
		// -90
		unsigned t = x;
		x = y;
		y = VECTOR_SERIAL_MAX - t;
	}

	uint32_t cmd = 0
		| (2 << 30)
		| (bright & 0x3F) << 24
		| (x & 0xFFF) << 12
		| (y & 0xFFF) <<  0
		;
	//printf("%08x %8d %8d %3d\n", cmd, x, y, intensity);

	m_serial_buf[m_serial_offset++] = cmd >> 24;
	m_serial_buf[m_serial_offset++] = cmd >> 16;
	m_serial_buf[m_serial_offset++] = cmd >>  8;
	m_serial_buf[m_serial_offset++] = cmd >>  0;

	// todo: check for overflow;
	// should always have enough points
    return 0;
}

void vector_device_v_st::serial_draw_line(float xf0,float yf0,float xf1,float yf1,int intensity)
{
	if (m_serial_fd < 0)
		return;

    rgb_t color = rgb_t(255, 255, 255);
	static int last_x;
	static int last_y;
	const int x0 = (xf0 * VECTOR_SERIAL_MAX - VECTOR_SERIAL_MAX/2) * m_serial_scale_x + m_serial_offset_x;
	const int y0 = (yf0 * VECTOR_SERIAL_MAX - VECTOR_SERIAL_MAX/2) * m_serial_scale_y + m_serial_offset_y;
	const int x1 = (xf1 * VECTOR_SERIAL_MAX - VECTOR_SERIAL_MAX/2) * m_serial_scale_x + m_serial_offset_x;
	const int y1 = (yf1 * VECTOR_SERIAL_MAX - VECTOR_SERIAL_MAX/2) * m_serial_scale_y + m_serial_offset_y;

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
    uint64_t size = 0;
    m_mirror = machine().config().options().vector_screen_mirror(); 
    std::error_condition filerr = osd_file::open(machine().config().options().vector_port(), OPEN_FLAG_READ | OPEN_FLAG_WRITE, m_serial, size);
   if (filerr)
    {
        fprintf(stderr, "vector_device_v_st: error: osd_file::open failed: %s on port %s\n" , const_cast<char*>(filerr.message().c_str()), machine().config().options().vector_port());
        ::exit(1);
    }
	/* Setup the serial output of the XY coords if configured */

	m_serial_scale = machine().options().vector_scale();
	m_serial_rotate = machine().options().vector_rotate();
	m_serial_bright = machine().options().vector_bright();
	m_serial_drop_frame = 0;
     
	if (m_serial_scale != 0.0)
	{
		// user specified a scale on the command line
		m_serial_scale_x = m_serial_scale_y = m_serial_scale;
	} else {
		// use the per-axis scales
		m_serial_scale_x = machine().options().vector_scale_x();
		m_serial_scale_y = machine().options().vector_scale_y();
	}

	m_serial_offset_x = machine().options().vector_offset_x();
	m_serial_offset_y = machine().options().vector_offset_y();
	// allocate enough buffer space, although we should never use this much
	m_serial_buf = make_unique_clear<uint8_t[]>((MAX_POINTS+2) * 4);
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

    m_serial_buf[m_serial_offset++] = 0;
	m_serial_buf[m_serial_offset++] = 0;
	m_serial_buf[m_serial_offset++] = 0;
	m_serial_buf[m_serial_offset++] = 0;
}
 
 
int vector_device_v_st::update(screen_device &screen, const rectangle &cliprect)
{
 
    serial_send();
    return 0;
}

