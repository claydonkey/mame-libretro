// license:BSD-3-Clause
// copyright-holders:Anthony Campbell, Trammel Hudson, Mario Montminy
#ifndef VECTOR_OVERRIDE_H
#define VECTOR_OVERRIDE_H
#pragma once

#include "video/vector.h"
#include "video/vector_device_t.h"
#include "emuopts.h"
#include "device.h"

class vector_override_device;

struct serial_segment_t
{
    struct serial_segment_t *next;
    int intensity;
    int x0;
    int y0;
    int x1;
    int y1;

    serial_segment_t(
        int x0,
        int y0,
        int x1,
        int y1,
        int intensity) : next(NULL),
                         intensity(intensity),
                         x0(x0),
                         y0(y0),
                         x1(x1),
                         y1(y1)
    {
    }
};

class vector_override_options
{
public:
    friend class vector_override_device;
    static float s_vector_scale;
    static float s_vector_scale_x;
    static float s_vector_scale_y;
    static float s_vector_offset_x;
    static float s_vector_offset_y;
    static char *s_vector_port;

    static int s_vector_rotate;
    static int s_vector_bright;

protected:
    static void init(emu_options &options);
};

class vector_override_device : public vector_device
{
protected:
    virtual void device_start() override;
    virtual void device_add_mconfig(machine_config &config) override;

public:
    vector_override_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock = 0);

    virtual uint32_t screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect) override;

    virtual void clear_list() override;
    virtual void add_point(int x, int y, rgb_t color, int intensity) override;
    // device-level overrides

    virtual void serial_draw_line(float xf0, float yf0, float xf1, float yf1, int intensity) override;

private:
    // Serial output option for driving vector display hardware
    osd_file::ptr m_serial;
    size_t m_serial_offset;
    std::unique_ptr<uint8_t[]> m_serial_buf;
    unsigned m_vector_transit[3];

    int m_serial_drop_frame;
    int m_serial_sort;
    struct serial_segment_t *m_serial_segments;
    struct serial_segment_t *m_serial_segments_tail;
    int serial_send();
    void serial_reset();
    int serial_read(uint8_t *buf, int size);
    std::error_condition serial_write(uint8_t *buf, int size);

    void serial_draw_point(unsigned x, unsigned y, int intensity);
};

// device type definition
DECLARE_DEVICE_TYPE(VECTOR_OVERRIDE, vector_override_device)
#endif // VECTOR_OVERRIDE_H
