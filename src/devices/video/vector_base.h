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
									else if (!strcmp(driver_name, "usb_v_st")) \
									{ \
										VECTOR_V_ST(config, opt_device); \
									} \

//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

class vector_device_base : public device_t
{
public:
	// construction/destruction
	vector_device_base(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock = 0);

	// 0 = continue, 1 = handled (skip)
	virtual int add_point(int x, int y, rgb_t color, int intensity);
	virtual int update(screen_device &screen, const rectangle &cliprect);

};



#endif // VECTOR_BASE_H