// license:BSD-3-Clause
// copyright-holders:Mario Montminy, Anthony Campbell
#ifndef VECTOR_BASE_H
#define VECTOR_BASE_H

#pragma once

#include "osdcore.h"
#include "screen.h"

#define VECTOR_DRIVER_INSTANTIATE(driver_name, config, opt_device) \
									if (!strcmp(driver_name, "usb_dvg")) \
									{ \
										VECTOR_USB_DVG(config, opt_device); \
									} \
									else if (!strcmp(driver_name, "v_st")) \
									{ \
										VECTOR_V_ST(config, opt_device); \
									} 

//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

class vector_device_t : public device_t , public device_video_interface
{
public:
	// construction/destruction
	vector_device_t(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock = 0);

	virtual void add_point(int x, int y, rgb_t color, int intensity);
	virtual uint32_t screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect) ;


protected:
virtual void device_start() override;
virtual void device_reset() override;
virtual void device_stop() override;
virtual void device_add_mconfig(machine_config &config) override;
};



#endif // VECTOR_BASE_H