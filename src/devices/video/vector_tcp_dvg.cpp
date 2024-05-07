// license:BSD-3-Clause
// copyright-holders:Mario Montminy, Anthony Campbell
#include "emu.h"
#include "emuopts.h"
#include "rendutil.h"
#include "video/vector.h"
#include "video/vector_udp_dvg.h"
#include "video/vector_tcp_dvg.h"
#include <rapidjson/document.h>
#include "rapidjson/prettywriter.h" // for stringify JSON
#include "logmacro.h"
#include <language.h>
#include <drivenum.h>
#include <cassert>
#include <ranges>
#include <chrono>
#include <thread>
#include "msgpack/msgpack.hpp"
#include <algorithm>
#include <iostream>
#include <string> 

#include <stdio.h>
#include <string.h>

using namespace std::chrono;
using namespace std::this_thread; // sleep_for, sleep_until
using namespace std::chrono_literals; // ns, us, ms, s, h, etc.
// ns, us, ms, s, h, etc.

using namespace util;
using namespace std;// Defining region codes 
#define LEFT                     0x1
#define RIGHT                    0x2
#define BOTTOM                   0x4
#define TOP                      0x8

#define HEADER_ADDITION          1
DEFINE_DEVICE_TYPE(VECTOR_TCP_DVG, vector_device_tcp_dvg, "vector_tcp_dvg", "VECTOR_TCP_DVG")




using namespace rapidjson;

vector_device_tcp_dvg::vector_device_tcp_dvg(const machine_config& mconfig, device_type type,const char* tag, device_t* owner, uint32_t clock)
	: vector_device_udp_dvg(mconfig,  type, tag, owner, clock)
{}
 


void vector_device_tcp_dvg::device_start() 
{
	rcvMBps = { 0 };
	std::string host(machine().config().options().vector_tcp_host());
	device_sethost("socket." + host + ":" + machine().config().options().vector_tcp_port());
	std::error_condition filerr = device_start_client();

	if (filerr.value())
	{
		fprintf(stderr, "vector_device_tcp_dvg: error: osd_file::open failed: %s tcp host %s\n", const_cast<char*>(filerr.message().c_str()), machine().config().options().vector_tcp_host());
		::exit(1);
	}

	m_json_buf = std::make_unique<uint8_t[]>(MAX_JSON_SIZE);
	m_json_length = 0;
	m_gamma_table = std::make_unique<float[]>(256);

	for (int i = 0; i < 256; i++)
	{
		m_gamma_table[i] = apply_brightness_contrast_gamma_fp(i, machine().options().brightness(), machine().options().contrast(), machine().options().gamma());
	}

	 determine_game_settings();
	m_clipx_min = DVG_RES_MIN;
	m_clipy_min = DVG_RES_MIN;
	m_clipx_max = DVG_RES_MAX;
	m_clipy_max = DVG_RES_MAX;
}

