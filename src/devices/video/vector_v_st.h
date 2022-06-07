// license:BSD-3-Clause
// copyright-holders:Mario Montminy, Anthony Campbell
#ifndef VECTOR_V_ST_H
#define VECTOR_V_ST_H
#pragma once

#include "osdcore.h"
#include "screen.h"
#include "vector_device_t.h"

class vector_device_v_st : public vector_device_t
{
public:
	vector_device_v_st(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);
	// device-level overrides

	virtual void add_point(int x, int y, rgb_t color, int intensity) override;
  virtual uint32_t screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect) override;

private:
    void serial_draw_point(unsigned x,unsigned y,int intensity);
	void serial_draw_line(float x0,float y0,float x1,float y1,int intensity);
	void serial_reset();
	int serial_write(uint8_t *buf, int size);
	int serial_read(uint8_t *buf, int size);
	int serial_send();
	osd_file::ptr               m_serial; 
    int m_serial_fd;
	float m_serial_scale;
	float m_serial_scale_x;
	float m_serial_scale_y;
	float m_serial_offset_x;
	float m_serial_offset_y;
	int m_serial_flip;
	int m_serial_rotate;
	int m_serial_bright;
	int m_serial_drop_frame;
	bool m_mirror;
	unsigned m_vector_transit[3];

    std::unique_ptr<unsigned char[]> m_serial_buf;
	size_t m_serial_offset;
protected:
    virtual void device_start() override;
    virtual void device_reset() override;
    virtual void device_stop() override;
};


// device type definition
DECLARE_DEVICE_TYPE(VECTOR_V_ST, vector_device_v_st)
#endif //VECTOR_V_ST_H