// license:BSD-3-Clause
// copyright-holders: Anthony Campbell
#include "emu.h"
#include "emuopts.h"
#include "rendutil.h"
#include "video/vector.h"
#include "video/vector_udp_dvg.h"
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
#include <lz4.h>
#include <lz4hc.h>


#include "video/ui.h"

#include <google/protobuf/util/time_util.h>
#include "video/ui.pb.h"

using namespace std::chrono;
using namespace std::this_thread; // sleep_for, sleep_until
using namespace std::chrono_literals; // ns, us, ms, s, h, etc.
// ns, us, ms, s, h, etc.

char c_header[SIZEOF_HEADER] = { '$', 'c', 'm', 'd', '_', 0, 0, 0, 0 };
char r_header[SIZEOF_HEADER] = { '$', 'r', 's', 'p', '_', 0, 0, 0, 0 };
using namespace util;
using namespace std;// Defining region codes 
#define LEFT                     0x1
#define RIGHT                    0x2
#define BOTTOM                   0x4
#define TOP                      0x8


DEFINE_DEVICE_TYPE(VECTOR_UDP_DVG, vector_device_udp_dvg, "vector_udp_dvg", "VECTOR_UDP_DVG")


const vector_device_udp_dvg::game_info_t vector_device_udp_dvg::s_games[] = {
	{"armora",   false, GAME_ARMORA,  true},
	{"armorap",  false, GAME_ARMORA,  true},
	{"armorar",  false, GAME_ARMORA,  true},
	{"asteroid", false, GAME_NONE,    true},
	{"asteroi1", false, GAME_NONE,    true},
	{"astdelux", false, GAME_NONE,    true},
	{"astdelu1", false, GAME_NONE,    true},
	{"llander",  false, GAME_NONE,    true},
	{"llander1", false, GAME_NONE,    true},
	{"demon",    false, GAME_NONE,    true},
	{"barrier",  false, GAME_NONE,    true},
	{"bzone",    false, GAME_NONE,    true},
	{"bzone2",   false, GAME_NONE,    true},
	{"bzonec",   false, GAME_NONE,    true},
	{"redbaron", false, GAME_NONE,    true},
	{"omegrace", false, GAME_NONE,    true},
	{"ripoff",   false, GAME_NONE,    true},
	{"solarq",   false, GAME_NONE,    true},
	{"speedfrk", false, GAME_NONE,    true},
	{"starhawk", false, GAME_NONE,    true},
	{"sundance", false, GAME_NONE,    true},
	{"tailg",    false, GAME_NONE,    true},
	{"warrior",  false, GAME_WARRIOR, true},
	{"wotw",     false, GAME_NONE,    true},
	{"spacewar", false, GAME_NONE,    true},
	{"starcas",  false, GAME_NONE,    true},
	{"starcas1", false, GAME_NONE,    true},
	{"starcasp", false, GAME_NONE,    true},
	{"starcase", false, GAME_NONE,    true},
	{"starwars", true, GAME_NONE,     false},
	{"starwar1", true, GAME_NONE,     false},
	{"esb",      true, GAME_NONE,     false},
	{ 0 }
};

using namespace rapidjson;



vector_device_udp_dvg::vector_device_udp_dvg(const machine_config& mconfig, const char* tag, device_t* owner, uint32_t clock)
	: vector_interface(mconfig, VECTOR_UDP_DVG, tag, owner, clock),
	m_exclude_blank_vectors(false),
	m_xmin(0),
	m_xmax(0),
	m_ymin(0),
	m_ymax(0),
	m_xscale(1.0),
	m_yscale(1.0),
	m_swap_xy(false),
	m_flip_x(false),
	m_flip_y(true),
	m_clipx_min(DVG_RES_MIN),
	m_clipx_max(DVG_RES_MAX),
	m_clipy_min(DVG_RES_MIN),
	m_clipy_max(DVG_RES_MAX),
	m_last_r(-1),
	m_last_g(-1),
	m_last_b(-1),
	m_artwork(0),
	m_bw_game(false),
	m_in_vec_last_x(0),
	m_in_vec_last_y(0),
	m_vertical_display(0),
	m_json_length(0)
{

}


void vector_device_udp_dvg::device_sethost(std::string _host) {
	m_host = _host;
}
std::error_condition vector_device_udp_dvg::device_start_client() {
	uint64_t size = 0;
	std::error_condition filerr = osd_file::open(m_host, OPEN_FLAG_READ | OPEN_FLAG_WRITE, m_port, size);
	m_json_length = 0;
	//	m_run = true;
	return filerr;
}


void vector_device_udp_dvg::device_start()
{
	rcvMBps = { 0 };
	std::string host(machine().config().options().vector_host());


	if (!strcmp(machine().config().options().vector_protocol(), "tcp")) {
		device_sethost("socket." + host + ":" + std::to_string(machine().config().options().vector_ip_port()));
		m_protocol = PROTO_TCP;
	}
	else if (!strcmp(machine().config().options().vector_protocol(), "udp"))
	{
		device_sethost("udp_socket." + host + ":" + std::to_string(machine().config().options().vector_ip_port()));
		m_protocol = PROTO_UDP;
	}
	else
	{
		fprintf(stderr, "vector_device_vectrx2020: error: vector_protocol %s unknown\n", machine().config().options().vector_protocol());
		::exit(1);
	}



	std::error_condition filerr = device_start_client();

	if (filerr.value())
	{
		fprintf(stderr, "vector_device_vectrx2020: error: osd_file::open failed: %s host %s protocol %s\n", const_cast<char*>(filerr.message().c_str()), machine().config().options().vector_host(), machine().config().options().vector_protocol());
		::exit(1);
	}

	m_json_buf = std::make_unique<uint8_t[]>(MAX_JSON_SIZE);
	m_json_length = 0;
	m_gamma_table = std::make_unique<float[]>(256);

	for (int i = 0; i < 256; i++)
	{
		m_gamma_table[i] = apply_brightness_contrast_gamma_fp(i, machine().options().brightness(), machine().options().contrast(), machine().options().gamma());
	}

	set_dvg_protocol();
	m_clipx_min = DVG_RES_MIN;
	m_clipy_min = DVG_RES_MIN;
	m_clipx_max = DVG_RES_MAX;
	m_clipy_max = DVG_RES_MAX;

	get_dvg_settings();
	int dwell = machine().config().options().vector_dwell();
	bool rgb = machine().config().options().vector_rgb();
	int draw_steps = machine().config().options().vector_draw_steps();
	int move_steps = machine().config().options().vector_move_steps();
	int compression_level = machine().config().options().vector_compression();
	int dac_period = machine().config().options().vector_dac_period();
	int x_scale = machine().config().options().vector_scale_x() * 100;
	int y_scale = machine().config().options().vector_scale_y() * 100;
	int x_offset = machine().config().options().vector_offset_x();
	int y_offset = machine().config().options().vector_offset_y();

	//int buffer_mode = machine().config().options().vector_buffer_mode();
	int buffer_type = machine().config().options().vector_buffer_type();

	std::cout << "HOST SETTINGS" << std::endl;
	std::cout << "-------------" << std::endl;
	std::cout << "rgb " << m_st.settings.rgb << std::endl;
	std::cout << "dwell " << m_st.settings.dwell << std::endl;
	std::cout << "draw_steps " << m_st.settings.draw_steps << std::endl;
	std::cout << "move_steps " << m_st.settings.move_steps << std::endl;
	std::cout << "compression_level " << m_st.settings.compressionLevel << std::endl;
	std::cout << "dac_period " << m_st.settings.dac_period << std::endl;
	std::cout << "x_scale " << m_st.settings.scalex << std::endl;
	std::cout << "y_scale " << m_st.settings.scaley << std::endl;
	std::cout << "x_offset " << m_st.settings.x_offset << std::endl;
	std::cout << "y_offset " << m_st.settings.y_offset << std::endl;
	std::cout << "buffer_mode " << m_st.settings.buffer_mode << std::endl;
	std::cout << "buffer_type " << m_st.settings.buffer_type << std::endl;
	std::cout << std::endl;

	std::cout << "CLIENT SETTINGS" << std::endl;
	std::cout << "---------------" << std::endl;
	std::cout << "rgb " << rgb << std::endl;
	std::cout << "draw_steps " << draw_steps << std::endl;
	std::cout << "move_steps " << move_steps << std::endl;
	std::cout << "compression_level " << compression_level << std::endl;
	std::cout << "dac_period " << dac_period << std::endl;
	std::cout << "x_scale " << x_scale << std::endl;
	std::cout << "y_scale " << y_scale << std::endl;
	std::cout << "x_offset " << x_offset << std::endl;
	std::cout << "y_offset " << y_offset << std::endl;
	std::cout << "dwell " << dwell << std::endl;
	//std::cout << "buffer_mode " << buffer_mode << std::endl;
	std::cout << "buffer_type " << buffer_type << std::endl;

	if (compression_level >= m_compression_level_t::LEVEL_NONE && compression_level <= m_compression_level_t::LEVEL_MAX) {
		m_st.settings.compressionLevel = static_cast<m_compression_level_t>(compression_level);

	}
	else {
		fprintf(stderr, "vector_device_vectrx2020: error: vector_compression_level %d unknown\n", machine().config().options().vector_compression());
		::exit(1);
	}

	if (x_scale != m_st.settings.scalex)
		m_st.settings.scalex = x_scale;

	if (y_scale != m_st.settings.scaley)
		m_st.settings.scaley = y_scale;

	if (dwell != m_st.settings.dwell)
		m_st.settings.dwell = dwell;

	if (buffer_type != m_st.settings.buffer_type)
		m_st.settings.buffer_type = static_cast<enum_buffer_type_t>(buffer_type);

	if (x_offset != m_st.settings.x_offset)
		m_st.settings.x_offset = x_offset;


	if (y_offset != m_st.settings.y_offset)
		m_st.settings.y_offset = y_offset;

	if (dwell != m_st.settings.dwell)
		m_st.settings.dwell = dwell;

	if (dac_period && dac_period != m_st.settings.dac_period)
		m_st.settings.dac_period = dac_period;

	if (rgb != m_st.settings.rgb)
		m_st.settings.rgb = rgb;

	if (move_steps && move_steps != m_st.settings.move_steps)
		m_st.settings.move_steps = move_steps;

	if (draw_steps && draw_steps != m_st.settings.draw_steps)
		m_st.settings.draw_steps = draw_steps;



	set_dvg_settings();
	get_dvg_info();
	send_game_info();
}

void vector_device_udp_dvg::device_stop_client() {
	std::error_condition filerr = osd_file::remove(m_host);

}

std::size_t vector_device_udp_dvg::get_dvg_settings()
{
	uint8_t buffer[SERVER_BUFF_SIZE];
	std::size_t result = 0;
	try
	{
		// Send the command to get settings
		std::size_t bytes_sent = writePacket(FLAG_GET_DVG_SETTINGS, nullptr, 0);
		if (bytes_sent <= 0)
		{
			// Handle error: failed to send command
			return -1;
		}

		// Read the response message
		result = readPacket(FLAG_GET_DVG_SETTINGS, buffer);
		if (result <= 0)
		{
			// Handle error: failed to read message or command mismatch
			return -1;
		}

		// At this point, buffer contains the settings payload
		std::cout << std::endl
			<< "buffer: " << buffer << std::endl;

		// Deserialize the settings from the buffer
		deserializeSettings(buffer, result);
	}
	catch (const std::exception& e)
	{
		std::cout << "get_dvg_settings " << e.what() << std::endl;

		return -1;
	}
	return result;
}


std::size_t vector_device_udp_dvg::set_dvg_settings()
{

	char* buffer = nullptr;
	std::size_t bytes_sent = -1;
	std::size_t len = 0;
	try
	{
		serializeSettings(&buffer, &len);
		if (buffer != nullptr)
		{
			bytes_sent = writePacket(FLAG_SET_DVG_SETTINGS, buffer, len);
			// Handle the bytes_sent as needed
			delete[] buffer; // Clean up the allocated buffer after sending
		}
	}
	catch (const std::exception& e)
	{
		std::cout << "set_dvg_settings " << e.what() << std::endl;

		return -1;
	}
	return bytes_sent;
}

// Assuming 'host_settings' is a member variable of VectrxManager or passed as a
// parameter
void vector_device_udp_dvg::serializeSettings(char** buffer, std::size_t* size)
{
	m_st::messageSettings settingsMsg;

	settingsMsg.set_font_type(static_cast<m_st::Font>(m_st.font_type));
	settingsMsg.set_page(static_cast<m_st::Pagetype>(m_st.page));

	m_st::Settings* protoSettings = settingsMsg.mutable_settings();

	protoSettings->set_dac_period(m_st.settings.dac_period);
	protoSettings->set_draw_steps(m_st.settings.draw_steps);
	protoSettings->set_x_offset(m_st.settings.x_offset);
	protoSettings->set_y_offset(m_st.settings.y_offset);
	protoSettings->set_dwell(m_st.settings.dwell);
	protoSettings->set_fine_type(m_st.settings.fine_type);
	protoSettings->set_controllers(m_st.settings.controllers);
	protoSettings->set_lcd(m_st.settings.lcd);
	protoSettings->set_audio(m_st.settings.audio);
	protoSettings->set_draw_jumps(m_st.settings.draw_jumps);
	protoSettings->set_auto_sync(m_st.settings.auto_sync);
	protoSettings->set_buffer_mode(static_cast<m_st::BufferMode>(m_st.settings.buffer_mode));
	protoSettings->set_buffer_type(static_cast<m_st::BufferType>(m_st.settings.buffer_type));
	protoSettings->set_scalex(m_st.settings.scalex);
	protoSettings->set_scaley(m_st.settings.scaley);
	protoSettings->set_orientation(static_cast<m_st::Orientation>(m_st.settings.orientation));
	protoSettings->set_ui_flags(m_st.settings.ui_flags);
	protoSettings->set_in_freertos(m_st.settings.in_freertos);
	protoSettings->set_protocol(static_cast<m_st::Protocol>(m_st.settings.protocol));
	protoSettings->set_compressionlevel(static_cast<m_st::compressionFlags>(m_st.settings.compressionLevel));
	protoSettings->set_rgb(m_st.settings.rgb);
	protoSettings->set_maxx(m_st.settings.maxx);
	protoSettings->set_maxy(m_st.settings.maxy);
	protoSettings->set_minx(m_st.settings.minx);
	protoSettings->set_miny(m_st.settings.miny);
	protoSettings->set_move_steps(m_st.settings.move_steps);


	// Serialize the Protobuf message to a byte buffer
	*size = settingsMsg.ByteSizeLong();
	*buffer = new char[*size]; // Allocate memory for the buffer
	if (!settingsMsg.SerializeToArray(*buffer, *size))
	{
		std::cerr << "Failed to serialize settings." << std::endl;
		delete[] * buffer; // Clean up the allocated buffer on failure
		*buffer = nullptr;
		*size = 0;
	}
}

void vector_device_udp_dvg::deserializeSettings(uint8_t* buffer, uint16_t size)
{
	m_st::messageSettings settingsMsg;
	std::string serialized_data(reinterpret_cast<char*>(buffer), size);
	if (settingsMsg.ParseFromString(serialized_data))
	{

		m_st.page = static_cast<m_pagetype_enum>(settingsMsg.page());
		m_st.font_type = static_cast<m_font_t>(settingsMsg.font_type());

		const m_st::Settings& protoSettings = settingsMsg.settings();
		m_st.settings.dac_period = protoSettings.dac_period();
		m_st.settings.draw_steps = protoSettings.draw_steps();
		m_st.settings.dwell = protoSettings.dwell();
		m_st.settings.x_offset = protoSettings.x_offset();
		m_st.settings.y_offset = protoSettings.y_offset();
		m_st.settings.fine_type = protoSettings.fine_type();
		m_st.settings.controllers = protoSettings.controllers();
		m_st.settings.lcd = protoSettings.lcd();
		m_st.settings.audio = protoSettings.audio();
		m_st.settings.draw_jumps = protoSettings.draw_jumps();
		m_st.settings.auto_sync = protoSettings.auto_sync();
		m_st.settings.buffer_mode = static_cast<enum_buffer_mode_t>(protoSettings.buffer_mode());
		m_st.settings.buffer_type = static_cast<enum_buffer_type_t>(protoSettings.buffer_type());
		m_st.settings.scalex = protoSettings.scalex();
		m_st.settings.scaley = protoSettings.scaley();
		m_st.settings.orientation = static_cast<m_orientation_enum>(protoSettings.orientation());
		m_st.settings.ui_flags = protoSettings.ui_flags();
		m_st.settings.in_freertos = protoSettings.in_freertos();
		m_st.settings.protocol = static_cast<m_protocol_enum_t>(protoSettings.protocol());
		m_st.settings.compressionLevel = static_cast<m_compression_level_t>(protoSettings.compressionlevel());
		m_st.settings.rgb = protoSettings.rgb();
		m_st.settings.move_steps = protoSettings.move_steps();
		m_st.settings.maxx = protoSettings.maxx();
		m_st.settings.maxy = protoSettings.maxy();
		m_st.settings.minx = protoSettings.minx();
		m_st.settings.miny = protoSettings.miny();
	
	}
	else
	{
		std::cerr << "Failed to parse settings." << std::endl;
	}
}

void vector_device_udp_dvg::device_off()
{
	device_stop();
}

void vector_device_udp_dvg::device_stop()
{

	std::size_t eot_bytes_sent = writePacket(FLAG_EXIT, nullptr, 0);
	if (eot_bytes_sent == static_cast<std::size_t>(-1))
	{
		std::cerr << "Failed to send device_stop ." << std::endl;
	}

}

uint32_t vector_device_udp_dvg::compute_code(int32_t x, int32_t y)
{
	// initialized as being inside 
	uint32_t code = 0;

	if (x < m_clipx_min)      // to the left of rectangle 
		code |= LEFT;
	else if (x > m_clipx_max) // to the right of rectangle 
		code |= RIGHT;
	if (y < m_clipy_min)      // below the rectangle 
		code |= BOTTOM;
	else if (y > m_clipy_max) // above the rectangle 
		code |= TOP;

	return code;
}

//
// Cohen-Sutherland line-clipping algorithm.  
//
uint32_t vector_device_udp_dvg::line_clip(int32_t* pX1, int32_t* pY1, int32_t* pX2, int32_t* pY2)
{
	int32_t x = 0, y = 0, x1, y1, x2, y2;
	uint32_t accept, code1, code2, code_out;

	x1 = *pX1;
	y1 = *pY1;
	x2 = *pX2;
	y2 = *pY2;

	accept = 0;
	// Compute region codes for P1, P2 
	code1 = compute_code(x1, y1);
	code2 = compute_code(x2, y2);

	while (1)
	{
		if ((code1 == 0) && (code2 == 0))
		{
			// If both endpoints lie within rectangle 
			accept = 1;
			break;
		}
		else if (code1 & code2)
		{
			// If both endpoints are outside rectangle, 
			// in same region 
			break;
		}
		else
		{
			// Some segment of line lies within the 
			// rectangle 
			// At least one endpoint is outside the 
			// rectangle, pick it. 
			if (code1 != 0)
			{
				code_out = code1;
			}
			else
			{
				code_out = code2;
			}

			// Find intersection point; 
			// using formulas y = y1 + slope * (x - x1), 
			// x = x1 + (1 / slope) * (y - y1) 
			if (code_out & TOP)
			{
				// point is above the clip rectangle 
				x = x1 + (x2 - x1) * (m_clipy_max - y1) / (y2 - y1);
				y = m_clipy_max;
			}
			else if (code_out & BOTTOM)
			{
				// point is below the rectangle 
				x = x1 + (x2 - x1) * (m_clipy_min - y1) / (y2 - y1);
				y = m_clipy_min;
			}
			else if (code_out & RIGHT)
			{
				// point is to the right of rectangle 
				y = y1 + (y2 - y1) * (m_clipx_max - x1) / (x2 - x1);
				x = m_clipx_max;
			}
			else if (code_out & LEFT)
			{
				// point is to the left of rectangle 
				y = y1 + (y2 - y1) * (m_clipx_min - x1) / (x2 - x1);
				x = m_clipx_min;
			}

			// Now intersection point x, y is found 
			// We replace point outside rectangle 
			// by intersection point 
			if (code_out == code1)
			{
				x1 = x;
				y1 = y;
				code1 = compute_code(x1, y1);
			}
			else
			{
				x2 = x;
				y2 = y;
				code2 = compute_code(x2, y2);
			}
		}
	}
	*pX1 = x1;
	*pY1 = y1;
	*pX2 = x2;
	*pY2 = y2;
	return accept;
}
void vector_device_udp_dvg::cmd_vec_postproc()
{
	int32_t  last_x = 0;
	int32_t  last_y = 0;
	int32_t  x0, y0, x1, y1;
	uint32_t add;


	m_out_vectors.clear();

	for (auto& vec : m_in_vectors)
	{
		x0 = vec.x0;
		y0 = vec.y0;
		x1 = vec.x1;
		y1 = vec.y1;
		rgb_t color = vec.color;
		if (vec.screen_coords)
		{
			transform_and_scale_coords(&x0, &y0);
			transform_and_scale_coords(&x1, &y1);
		}
		add = line_clip(&x0, &y0, &x1, &y1);
		if (add)
		{
			if (last_x != x0 || last_y != y0)
			{
				vector_t vec = { last_x, last_y, x0, y0, rgb_t(0, 0, 0) };
				// Disconnect detected. Insert a blank vector.
				m_out_vectors.push_back(vec);
			}
			vector_t vec2 = { last_x, last_y, x1, y1,color };
			m_out_vectors.push_back(vec2);

		}
		last_x = x1;
		last_y = y1;
	}
}



//
// Reset the indexes to the vector list and command buffer.
//
void vector_device_udp_dvg::cmd_reset(uint32_t initial)
{
	m_in_vec_last_x = 0;
	m_in_vec_last_y = 0;
	out_meta.clear();
	out_data.clear();
	m_in_vectors.clear();
	m_out_vectors.clear();

}

void vector_device_udp_dvg::cmd_add_vec(int x, int y, rgb_t color, bool screen_coords)
{

	int32_t    x0, y0, x1, y1;
	bool blank;
	x0 = m_in_vec_last_x;
	y0 = m_in_vec_last_y;
	x1 = x;
	y1 = y;
	bool add = true;
	blank = (color.r() == 0) && (color.g() == 0) && (color.b() == 0);
	if ((x1 == x0) && (y1 == y0) && blank)
	{
		add = false;
	}
	if (m_exclude_blank_vectors)
	{
		if (add)
		{
			add = !blank;
		}
	}
	if (add)
	{

		if (m_in_vectors.size() < MAX_VECTORS)
			m_in_vectors.push_back({ x0, y0, x1, y1, color, screen_coords });
	}
	m_in_vec_last_x = x;
	m_in_vec_last_y = y;
}


void  vector_device_udp_dvg::transform_and_scale_coords(int* px, int* py)
{
	float x, y;

	x = (*px >> 16) + (*px & 0xffff) / 65536.0;
	x *= m_xscale;
	y = (*py >> 16) + (*py & 0xffff) / 65536.0;
	y *= m_yscale;
	*px = x;
	*py = y;
}


void vector_device_udp_dvg::get_dvg_info()
{
	uint32_t i;
	Document d;

	int      result;
	uint32_t version, major, minor;
	pkt_cnt = 0;
	if (m_json_length)
	{
		return;
	}

	sscanf(emulator_info::get_build_version(), "%u.%u", &major, &minor);
	version = (((minor / 1000) % 10) << 12) | (((minor / 100) % 10) << 8) | (((minor / 10) % 10) << 4) | (minor % 10);
	writePacket(FLAG_GET_DVG_INFO, reinterpret_cast<const char*>(&version), sizeof(version));
	result = readPacket(FLAG_GET_DVG_INFO, reinterpret_cast<uint8_t*>(&m_recv_buffer));


	if (result <= 0)
	{
		std::cerr << "Failed to receive packet data." << std::endl;
		return;
	}

	m_flip_y = false;
	d.Parse(reinterpret_cast<const char*> (&m_json_buf[0]));
	m_vertical_display = d["vertical"].GetBool();

	if (machine().config().gamedrv().flags & machine_flags::SWAP_XY)
	{
		std::cout << "SWAP_XY " << std::endl;
		m_swap_xy = true;
	}
	if (machine().config().gamedrv().flags & machine_flags::FLIP_Y)
	{
		std::cout << "FLIP_Y " << std::endl;
		m_flip_y = true;
	}
	if (machine().config().gamedrv().flags & machine_flags::FLIP_X)
	{
		std::cout << "FLIP_X " << std::endl;
		m_flip_x = true;
	}

	m_bw_game = false;
	m_artwork = GAME_NONE;
	m_exclude_blank_vectors = false;
	if (!strcmp(emulator_info::get_appname_lower(), "mess"))
	{
		m_bw_game = 1;
	}
	else
	{
		for (i = 0; s_games[i].name; i++)
		{
			if (!strcmp(machine().system().name, s_games[i].name))
			{
				m_artwork = s_games[i].artwork;
				m_exclude_blank_vectors = s_games[i].exclude_blank_vectors;
				m_bw_game = s_games[i].bw_game;
				break;
			}
		}
	}
	//todo: check if need for RGB currently mhavoc needs this to be true
	m_exclude_blank_vectors = true;

}


void vector_device_udp_dvg::transform_final(int* px, int* py)
{
	int x, y, tmp;
	x = *px;
	y = *py;

	if (m_swap_xy)
	{
		tmp = x;
		x = y;
		y = tmp;
	}
	if (m_flip_x)
	{
		x = DVG_RES_MAX - x;
	}
	if (m_flip_y)
	{
		y = DVG_RES_MAX - y;
	}
	x = std::clamp(x, 0, DVG_RES_MAX);
	y = std::clamp(y, 0, DVG_RES_MAX);

	if (m_vertical_display)
	{
		if (!m_swap_xy)
		{

			y = 512 + (0.75 * y);
		}
	}
	else
	{
		if (m_swap_xy)
		{
			// Vertical on horizontal display
			x = 512 + (0.75 * x);
		}

	}

	*px = x;
	*py = y;
}


uint64_t ByteToint64(uint8_t a[], uint64_t* n) {
	memcpy((uint64_t*)n, a, 8);
	return *n;

}
uint32_t ByteToint32(uint8_t a[], uint32_t* n) {
	memcpy((uint32_t*)n, a, 4);
	return *n;

}

uint16_t ByteToint16(uint8_t a[], uint16_t* n) {
	memcpy((uint32_t*)n, a, 2);
	return *n;

}
uint8_t* int64ToByte(uint8_t a[], uint64_t n) {
	memcpy(a, &n, 8);
	return a;
}
uint8_t* int32ToByte(uint8_t a[], uint32_t n) {
	memcpy(a, &n, 4);
	return a;
}


uint8_t* int16ToByte(uint8_t a[], uint16_t n) {
	memcpy(a, &n, 2);
	return a;
}

uint64_t lineno = 0;
#if MAME_DEBUG

uint64_t lineno2 = 0;
#endif
extern "C" {
	typedef struct {
		uint32_t rgb : 24;
		uint32_t points : 24;

	}ds_v_colors_t;

	typedef  struct _colors_t {
		uint8_t b : 8;
		uint8_t g : 8;
		uint8_t r : 8;
	} ds_colors_t;

	typedef struct {
		ds_colors_t rgb;
		uint16_t y : 12;
		uint16_t x : 12;

	}	ds_point_t;
	}


int dctr = 0;
extern "C"  typedef   union {
	ds_point_t pnt;
	ds_v_colors_t colors;
}ds_dvg_vec;

uint32_t last_rgb = 0xFFFFFFFF; // Initialize with an impossible color


int vector_device_udp_dvg::send_vectors()
{

	uint8_t meta_byte = 0;
	size_t out_bit_iter = 0;
	cmd_vec_postproc();
	int bytes_sent = 0;
#if 1 ==0
	for (const auto& dvg : m_out_vectors)
	{


		uint32_t rgb = ((dvg.color.r() & 0xff) << 16) | ((dvg.color.g() & 0xff) << 8) | ((dvg.color.b() & 0xff));
		int32_t y1 = dvg.y1;
		int32_t x1 = dvg.x1;
		transform_final(&x1, &y1);
		// Scale and clamp the x and y coordinates
		uint32_t x = static_cast<uint32_t>(x1);
		uint32_t y = static_cast<uint32_t>(y1);
		// std::cout << "serialized point " << ": (" << dvg.pnt.y  << ", " << dvg.pnt.x  << ")\n";

		bool color_change = (rgb != last_rgb);

		// Update the meta byte
		meta_byte |= (color_change << (7 - out_bit_iter % 8));

		// Pack the color if it has changed
		if (color_change)
		{
			out_data.push_back(dvg.color.r() & 0xff); // Red
			out_data.push_back(dvg.color.g() & 0xff); // Green
			out_data.push_back(dvg.color.b() & 0xff); // Blue
			last_rgb = rgb;
		}

		// Pack the x and y coordinates
		out_data.push_back((x >> 4) & 0xFF);                     // Higher 8 bits of x
		out_data.push_back(((x & 0xF) << 4) | ((y >> 8) & 0xF)); // Lower 4 bits of x and higher 4 bits of y
		out_data.push_back(y & 0xFF);                            // Lower 8 bits of y

		// Push the meta byte to out_meta every 8 points or at the end
		if (++out_bit_iter % 8 == 0 || &dvg == &m_out_vectors.back())
		{
			out_meta.push_back(meta_byte);
			meta_byte = 0;
		}


	}
#endif

	uint32_t r, g, b, x, y, rgb;
	for (const auto& dvg : m_out_vectors)
	{
		r = (dvg.color.r() & 0xff) >> 4;
		g = (dvg.color.g() & 0xff) >> 4;
		b = (dvg.color.b() & 0xff) >> 4;

		rgb = (r << 8) | (g << 4) | b;

		int32_t y1 = dvg.y1;
		int32_t x1 = dvg.x1;
		transform_final(&x1, &y1);
		// Scale and clamp the x and y coordinates
		x = static_cast<uint32_t>(x1);
		y = static_cast<uint32_t>(y1);
		// std::cout << "serialized point " << ": (" << dvg.pnt.y  << ", " << dvg.pnt.x  << ")\n";

		bool color_change = (rgb != last_rgb);

		// Update the meta byte
		meta_byte |= (color_change << (7 - out_bit_iter % 8));

		// Pack the color if it has changed
		if (color_change)
		{
			out_data.push_back((r << 4) | (g & 0xF)); // Higher 4 bits of R and G
			out_data.push_back((b & 0xF));       // Higher 4 bits of B
			last_rgb = rgb;
		}

		// Pack the x and y coordinates
		out_data.push_back((x >> 4) & 0xFF);                     // Higher 8 bits of x
		out_data.push_back(((x & 0xF) << 4) | ((y >> 8) & 0xF)); // Lower 4 bits of x and higher 4 bits of y
		out_data.push_back(y & 0xFF);                            // Lower 8 bits of y

		// Push the meta byte to out_meta every 8 points or at the end
		if (++out_bit_iter % 8 == 0 || &dvg == &m_out_vectors.back())
		{
			out_meta.push_back(meta_byte);
			meta_byte = 0;
		}


	}


	std::string data_to_send(out_data.begin(), out_data.end());
	std::string meta_to_send(out_meta.begin(), out_meta.end());
	std::string compressed_data;
	cmd_reset(0);
	if (!compressData(data_to_send, compressed_data, static_cast<m_compression_level_t>(m_st.settings.compressionLevel)))
	{
		return -1;
	}
	else

		return sendFrame(meta_to_send, compressed_data);

}
bool vector_device_udp_dvg::compressData(const std::string& input_data, std::string& compressed_data, m_compression_level_t compression_level)
{
	if (m_st.settings.compressionLevel == m_compression_level_t::LEVEL_NONE)
	{
		compressed_data = input_data;
		return true;
	}

	const int max_compressed_size = LZ4_compressBound(input_data.size());
	std::vector<char> compressed_data_buffer(sizeof(int) + max_compressed_size);

	int decompressed_size = input_data.size();
	std::memcpy(compressed_data_buffer.data(), &decompressed_size, sizeof(int));

	int compressed_data_size = 0;
	if (m_st.settings.compressionLevel == m_compression_level_t::LEVEL_SAFE)
	{
		compressed_data_size = LZ4_compress_default(
			input_data.data(), compressed_data_buffer.data() + sizeof(int),
			input_data.size(), max_compressed_size);
	}
	else if (m_st.settings.compressionLevel > m_compression_level_t::LEVEL_SAFE)
	{
		compressed_data_size = LZ4_compress_HC(
			input_data.data(), compressed_data_buffer.data() + sizeof(int),
			input_data.size(), max_compressed_size, m_st.settings.compressionLevel);
	}

	if (compressed_data_size > 0)
	{
		compressed_data_buffer.resize(sizeof(int) + compressed_data_size);
		compressed_data.assign(compressed_data_buffer.begin(), compressed_data_buffer.end());
		return true;
	}
	else
	{
		std::cerr << "Failed to compress data with LZ4." << std::endl;
		return false;
	}
}
int vector_device_udp_dvg::sendFrame(const std::string& meta_data, const std::string& point_data)
{

	int packets_sent = 0;

	auto sendDataChunks = [this, &packets_sent](const std::string& data, cmd_enum cmd) -> std::size_t {
		std::size_t total_bytes_sent = 0;
		std::size_t offset = 0;
		const std::size_t data_size = data.size();

		while (offset < data_size)
		{
			std::size_t chunk_size = std::min(static_cast<std::size_t>(SERVER_DATA_SIZE), data_size - offset);
			packets_sent++;
			auto bytes_sent = writePacket(cmd, data.data() + offset, chunk_size);
			if (bytes_sent == static_cast<std::size_t>(-1))
			{
				return -1;
			}
			total_bytes_sent += bytes_sent;
			offset += chunk_size;
		}
		return total_bytes_sent;
		};


	std::size_t data_bytes_sent = sendDataChunks(point_data, FLAG_DVG_POINTS);
	if (data_bytes_sent == static_cast<std::size_t>(-1))
	{
		return -1;
	}

	uint32_t point_data_size = htonl(static_cast<uint32_t>(point_data.size()));
	std::string meta_data_with_size(reinterpret_cast<const char*>(&point_data_size), sizeof(point_data_size));
	meta_data_with_size += meta_data;

	std::size_t meta_bytes_sent = sendDataChunks(meta_data_with_size, FLAG_META);

	if (meta_bytes_sent == static_cast<std::size_t>(-1))
	{
		std::cout << "ERROR:: meta_bytes_sent" << std::endl;
		return -1;
	}
	//std::cout << "meta_bytes_sent: " << meta_data.size() << std::endl;

	std::size_t eot_bytes_sent = sendEOT(meta_data.size(), FLAG_EOT);
	std::size_t total_bytes_sent = meta_bytes_sent + data_bytes_sent + eot_bytes_sent;

	return total_bytes_sent;
}

std::size_t vector_device_udp_dvg::sendEOT(std::size_t data_size, cmd_enum cmd)
{
	uint32_t network_order_size = htonl(static_cast<uint32_t>(data_size));
	std::size_t eot_bytes_sent = writePacket(cmd, reinterpret_cast<char*>(&network_order_size), sizeof(network_order_size));
	if (eot_bytes_sent == static_cast<std::size_t>(-1))
	{
		return -1;
	}
	return eot_bytes_sent;
}


std::size_t vector_device_udp_dvg::readPacket(const cmd_enum command, uint8_t* message)
{
	if (!message)
	{
		return -1;
	}

	uint32_t bytes_read;

	try
	{
		m_port->read((char*)message, 0, SERVER_BUFF_SIZE, bytes_read);
		while (!bytes_read)
			m_port->read((char*)message, 0, SERVER_BUFF_SIZE, bytes_read);
		if (bytes_read == 0)
			return -1;
	}
	catch (const std::exception& e)
	{
		std::cout << "read failed " << e.what() << std::endl;
		return -1;
	}
	if (bytes_read >= SIZEOF_HEADER && strncmp((const char*)message, "$rsp", 4) == 0 && message[4] == command)
	{
		uint32_t payload_length_network;
		memcpy(&payload_length_network, message + 5, sizeof(payload_length_network));
		uint32_t payload_length = ntohl(payload_length_network);
		if ((bytes_read - SIZEOF_HEADER) != payload_length)
		{
			return 0;
		}
		memmove(message, message + SIZEOF_HEADER, payload_length);
		return payload_length;
	}

	return -1;
}

std::size_t vector_device_udp_dvg::writePacket(const cmd_enum command, const char* data, std::size_t data_size)
{

	uint32_t bytes_sent = 0;
	c_header[4] = static_cast<char>(command);
	uint32_t payload_length = htonl(data_size);
	std::memcpy(c_header + MSG_IDX_START, &payload_length, sizeof(payload_length));
	std::vector<char> packet(SIZEOF_HEADER + data_size);
	std::memcpy(packet.data(), c_header, SIZEOF_HEADER);

	try
	{
		if (data && data_size > 0)
		{
			std::memcpy(packet.data() + SIZEOF_HEADER, data, data_size);
		}

		m_port->write(packet.data(), 0, packet.size(), bytes_sent);

		if (bytes_sent != packet.size())
		{
			std::cerr << "Failed to send the packet." << std::endl;
			return -1;
		}
	}
	catch (const std::exception& e)
	{
		std::cout << "Failed to send the packet: " << e.what() << std::endl;

		return -1;
	}

	return static_cast<std::size_t>(bytes_sent);
}
void vector_device_udp_dvg::device_reset()
{
}
void vector_device_udp_dvg::add_point(int x, int y, rgb_t color, int intensity)
{
	intensity = std::clamp(intensity, 0, 255);
	if (intensity == 0)
	{
		color.set_r(0);
		color.set_g(0);
		color.set_b(0);
	}
	else
	{
		float cscale = m_gamma_table[intensity];
		color.set_r(cscale * color.r());
		color.set_g(cscale * color.g());
		color.set_b(cscale * color.b());
	}
	cmd_add_vec(x, y, color, true);
}
void vector_device_udp_dvg::set_dvg_protocol()
{

	printf("protocol %d\n", m_protocol);
	writePacket(FLAG_SET_PROTOCOL, std::to_string(static_cast<int>(m_protocol)).c_str(), 1);
}
std::string replaceAll(std::string str, const std::string& from, const std::string& to) {
	size_t start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
		str.replace(start_pos, from.length(), to);
		start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
	}
	return str;
}



void  vector_device_udp_dvg::send_game_info() {
	rapidjson::StringBuffer sb;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);
	GameDetails(machine()).Serialize(writer);
	const char* json = sb.GetString();
	uint16_t json_len = static_cast<uint16_t>(sb.GetSize());
	std::cout << "Game info: " << json << std::endl;
	writePacket(FLAG_GET_GAME_INFO, json, json_len);
}
uint32_t orientation_flags;
float xscale;
float yscale;
uint32_t vector_device_udp_dvg::screen_update(screen_device& screen, bitmap_rgb32& bitmap, const rectangle& cliprect)
{
	rgb_t color = rgb_t(108, 108, 108);
	rgb_t black = rgb_t(0, 0, 0);
	int x0, y0, x1, y1;


	/*
	if (orientation_flags != screen.orientation()) {
		orientation_flags = screen.orientation();
		if (orientation_flags & ORIENTATION_SWAP_XY) {
			m_st.settings.orientation = FLIP_XY;
		}
		else 	if (orientation_flags & ORIENTATION_FLIP_Y) {
			m_st.settings.orientation = FLIP_Y;
		}
		else 	if (orientation_flags & ORIENTATION_FLIP_X) {
			m_st.settings.orientation = FLIP_X;
		}
		else 	if (orientation_flags & ROT90) {
			m_st.settings.orientation = ROT_90;
		}
		else 	if (orientation_flags & ROT180) {
			m_st.settings.orientation = ROT_180;
		}
		else 	if (orientation_flags & ROT270) {
			m_st.settings.orientation = ROT_270;
		}
		else 	if (orientation_flags & ROT0) {
			m_st.settings.orientation = DEFAULT_ORIENTATION;
		}
		std::cout << "rotation" << std::endl;
		set_dvg_settings();
	}
	*/
	/*
	if (yscale != screen.yscale()) {
		yscale = screen.yscale();

		m_st.settings.scaley = 100 * yscale;

		std::cout << "yscale" << std::endl;
		set_dvg_settings();
	}
	if (xscale != screen.xscale()) {
		xscale = screen.xscale();

		m_st.settings.scalex = 100 * xscale;

		std::cout << "xscale" << std::endl;
		set_dvg_settings();
	}
	*/
	m_xmin = screen.visible_area().min_x;
	m_xmax = screen.visible_area().max_x;
	m_ymin = screen.visible_area().min_y;
	m_ymax = screen.visible_area().max_y;

	if (m_xmax == 0)
	{
		m_xmax = 1;
	}
	if (m_ymax == 0)
	{
		m_ymax = 1;
	}

	m_xscale = (DVG_RES_MAX + 1.0) / (m_xmax - m_xmin);
	m_yscale = (DVG_RES_MAX + 1.0) / (m_ymax - m_ymin);
	
	x0 = 0;
	y0 = 0;
	x1 = (cliprect.right() - cliprect.left()) * m_xscale;
	y1 = (cliprect.bottom() - cliprect.top()) * m_yscale;

	// Make sure the clip coordinates fall within the display coordinates.
	x0 = std::clamp(x0, 0, DVG_RES_MAX);
	y0 = std::clamp(y0, 0, DVG_RES_MAX);
	x1 = std::clamp(x1, 0, DVG_RES_MAX);
	y1 = std::clamp(y1, 0, DVG_RES_MAX);

	m_clipx_min = x0;
	m_clipy_min = y0;
	m_clipx_max = x1;
	m_clipy_max = y1;

	if (m_in_vectors.size())
	{
		switch (m_artwork) {
		case GAME_ARMORA:
			// Upper Right Quadrant
			// Outer structure
			cmd_add_vec(3446, 2048, black, false);
			cmd_add_vec(3958, 2224, color, false);
			cmd_add_vec(3958, 3059, color, false);
			cmd_add_vec(3323, 3059, color, false);
			cmd_add_vec(3323, 3225, color, false);
			cmd_add_vec(3194, 3225, color, false);
			cmd_add_vec(3194, 3393, color, false);
			cmd_add_vec(3067, 3393, color, false);
			cmd_add_vec(3067, 3901, color, false);
			cmd_add_vec(2304, 3901, color, false);
			cmd_add_vec(2304, 3225, color, false);
			cmd_add_vec(2048, 3225, color, false);
			// Center structure
			cmd_add_vec(2048, 2373, black, false);
			cmd_add_vec(2562, 2738, color, false);
			cmd_add_vec(2430, 2738, color, false);
			cmd_add_vec(2430, 2893, color, false);
			cmd_add_vec(2306, 2893, color, false);
			cmd_add_vec(2306, 3065, color, false);
			cmd_add_vec(2048, 3065, color, false);
			// Big structure
			cmd_add_vec(2938, 2209, black, false);
			cmd_add_vec(3198, 2383, color, false);
			cmd_add_vec(3706, 2383, color, false);
			cmd_add_vec(3706, 2738, color, false);
			cmd_add_vec(2938, 2738, color, false);
			cmd_add_vec(2938, 2209, color, false);
			// Small structure
			cmd_add_vec(2551, 3055, black, false);
			cmd_add_vec(2816, 3590, color, false);
			cmd_add_vec(2422, 3590, color, false);
			cmd_add_vec(2422, 3231, color, false);
			cmd_add_vec(2555, 3231, color, false);
			cmd_add_vec(2555, 3055, color, false);
			// Upper Left Quadrant
			// Outer structure
			cmd_add_vec(649, 2048, black, false);
			cmd_add_vec(137, 2224, color, false);
			cmd_add_vec(137, 3059, color, false);
			cmd_add_vec(772, 3059, color, false);
			cmd_add_vec(772, 3225, color, false);
			cmd_add_vec(901, 3225, color, false);
			cmd_add_vec(901, 3393, color, false);
			cmd_add_vec(1028, 3393, color, false);
			cmd_add_vec(1028, 3901, color, false);
			cmd_add_vec(1792, 3901, color, false);
			cmd_add_vec(1792, 3225, color, false);
			cmd_add_vec(2048, 3225, color, false);
			// Center structure
			cmd_add_vec(2048, 2373, black, false);
			cmd_add_vec(1533, 2738, color, false);
			cmd_add_vec(1665, 2738, color, false);
			cmd_add_vec(1665, 2893, color, false);
			cmd_add_vec(1789, 2893, color, false);
			cmd_add_vec(1789, 3065, color, false);
			cmd_add_vec(2048, 3065, color, false);
			// Big structure
			cmd_add_vec(1157, 2209, black, false);
			cmd_add_vec(897, 2383, color, false);
			cmd_add_vec(389, 2383, color, false);
			cmd_add_vec(389, 2738, color, false);
			cmd_add_vec(1157, 2738, color, false);
			cmd_add_vec(1157, 2209, color, false);
			// Small structure
			cmd_add_vec(1544, 3055, black, false);
			cmd_add_vec(1280, 3590, color, false);
			cmd_add_vec(1673, 3590, color, false);
			cmd_add_vec(1673, 3231, color, false);
			cmd_add_vec(1540, 3231, color, false);
			cmd_add_vec(1540, 3055, color, false);
			// Lower Right Quadrant
			// Outer structure
			cmd_add_vec(3446, 2048, black, false);
			cmd_add_vec(3958, 1871, color, false);
			cmd_add_vec(3958, 1036, color, false);
			cmd_add_vec(3323, 1036, color, false);
			cmd_add_vec(3323, 870, color, false);
			cmd_add_vec(3194, 870, color, false);
			cmd_add_vec(3194, 702, color, false);
			cmd_add_vec(3067, 702, color, false);
			cmd_add_vec(3067, 194, color, false);
			cmd_add_vec(2304, 194, color, false);
			cmd_add_vec(2304, 870, color, false);
			cmd_add_vec(2048, 870, color, false);
			// Center structure
			cmd_add_vec(2048, 1722, black, false);
			cmd_add_vec(2562, 1357, color, false);
			cmd_add_vec(2430, 1357, color, false);
			cmd_add_vec(2430, 1202, color, false);
			cmd_add_vec(2306, 1202, color, false);
			cmd_add_vec(2306, 1030, color, false);
			cmd_add_vec(2048, 1030, color, false);
			// Big structure
			cmd_add_vec(2938, 1886, black, false);
			cmd_add_vec(3198, 1712, color, false);
			cmd_add_vec(3706, 1712, color, false);
			cmd_add_vec(3706, 1357, color, false);
			cmd_add_vec(2938, 1357, color, false);
			cmd_add_vec(2938, 1886, color, false);
			// Small structure
			cmd_add_vec(2551, 1040, black, false);
			cmd_add_vec(2816, 505, color, false);
			cmd_add_vec(2422, 505, color, false);
			cmd_add_vec(2422, 864, color, false);
			cmd_add_vec(2555, 864, color, false);
			cmd_add_vec(2555, 1040, color, false);
			// Lower Left Quadrant
			// Outer structure
			cmd_add_vec(649, 2048, black, false);
			cmd_add_vec(137, 1871, color, false);
			cmd_add_vec(137, 1036, color, false);
			cmd_add_vec(772, 1036, color, false);
			cmd_add_vec(772, 870, color, false);
			cmd_add_vec(901, 870, color, false);
			cmd_add_vec(901, 702, color, false);
			cmd_add_vec(1028, 702, color, false);
			cmd_add_vec(1028, 194, color, false);
			cmd_add_vec(1792, 194, color, false);
			cmd_add_vec(1792, 870, color, false);
			cmd_add_vec(2048, 870, color, false);
			// Center structure
			cmd_add_vec(2048, 1722, black, false);
			cmd_add_vec(1533, 1357, color, false);
			cmd_add_vec(1665, 1357, color, false);
			cmd_add_vec(1665, 1202, color, false);
			cmd_add_vec(1789, 1202, color, false);
			cmd_add_vec(1789, 1030, color, false);
			cmd_add_vec(2048, 1030, color, false);
			// Big structure
			cmd_add_vec(1157, 1886, black, false);
			cmd_add_vec(897, 1712, color, false);
			cmd_add_vec(389, 1712, color, false);
			cmd_add_vec(389, 1357, color, false);
			cmd_add_vec(1157, 1357, color, false);
			cmd_add_vec(1157, 1886, color, false);
			// Small structure
			cmd_add_vec(1544, 1040, black, false);
			cmd_add_vec(1280, 505, color, false);
			cmd_add_vec(1673, 505, color, false);
			cmd_add_vec(1673, 864, color, false);
			cmd_add_vec(1540, 864, color, false);
			cmd_add_vec(1540, 1040, color, false);
			break;

		case GAME_WARRIOR:
			cmd_add_vec(1187, 2232, black, false);
			cmd_add_vec(1863, 2232, color, false);
			cmd_add_vec(1187, 1372, black, false);
			cmd_add_vec(1863, 1372, color, false);
			cmd_add_vec(1187, 2232, black, false);
			cmd_add_vec(1187, 1372, color, false);
			cmd_add_vec(1863, 2232, black, false);
			cmd_add_vec(1863, 1372, color, false);
			cmd_add_vec(2273, 2498, black, false);
			cmd_add_vec(2949, 2498, color, false);
			cmd_add_vec(2273, 1658, black, false);
			cmd_add_vec(2949, 1658, color, false);
			cmd_add_vec(2273, 2498, black, false);
			cmd_add_vec(2273, 1658, color, false);
			cmd_add_vec(2949, 2498, black, false);
			cmd_add_vec(2949, 1658, color, false);
			break;
		}
		
		send_vectors();
	}
	return 0;
}
