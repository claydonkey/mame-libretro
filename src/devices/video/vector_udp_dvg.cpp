// license:BSD-3-Clause
// copyright-holders:Mario Montminy, Anthony Campbell
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

void vector_device_udp_dvg::device_stop_client() {
	std::error_condition filerr = osd_file::remove(m_host);

}

void vector_device_udp_dvg::device_start()
{
	rcvMBps = { 0 };
	std::string host(machine().config().options().vector_udp_host());
	device_sethost("udp_socket." + host + ":2390");
	std::error_condition filerr = device_start_client();

	if (filerr.value())
	{
		fprintf(stderr, "vector_device_udp_dvg: error: osd_file::open failed: %s udp host %s\n", const_cast<char*>(filerr.message().c_str()), machine().config().options().vector_udp_host());
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
void vector_device_udp_dvg::device_stop()
{
	uint32_t cmd;

	cmd = (FLAG_EXIT << 29);

	//packets_write(&m_cmd_buf[0], m_cmd_offs);
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

 
	blank = (color.r() == 0) && (color.g() == 0) && (color.b() == 0);
	if (!(m_exclude_blank_vectors || ((x1 == x0) && (y1 == y0) && blank)))
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


int vector_device_udp_dvg::determine_game_settings()
{
	uint32_t i;
	Document d;

	get_dvg_info();

	d.Parse(reinterpret_cast<const char*> (&m_json_buf[0]));
	m_vertical_display = d["vertical"].GetBool();

	if (machine().config().gamedrv().flags & machine_flags::SWAP_XY)
	{
		m_swap_xy = true;
	}
	if (machine().config().gamedrv().flags & machine_flags::FLIP_Y)
	{
		m_flip_y = false;
	}
	if (machine().config().gamedrv().flags & machine_flags::FLIP_X)
	{
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
	return 0;
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


int vector_device_udp_dvg::packet_read(uint8_t* buf, int size)
{
	int result = size;
	uint32_t read = 0;

	m_port->read(buf, 0, size, read);
	while (!read)
		m_port->read(buf, 0, size, read);

	if (read != size)
	{
		result = -1;
	}
	return result;
}


int vector_device_udp_dvg::packets_write(uint8_t* buf, int size)
{

	uint32_t written = 0, total;
	total = size;
	m_port->write(buf, 0, size, written);
	return written;
}



uint64_t ByteToint64(uint8_t a[], uint64_t* n) {
	memcpy((uint64_t*)n, a, 8);
	return *n;

}
uint32_t ByteToint32(uint8_t a[], uint32_t* n) {
	memcpy((uint32_t*)n, a, 4);
	return *n;

}
void int64ToByte(uint8_t a[], uint64_t n) {
	memcpy(a, &n, 8);

}
void int32ToByte(uint8_t a[], uint32_t n) {
	memcpy(a, &n, 4);

}
uint8_t c_header[] = { '$','c','m','d','_','$' };
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

 int32_t vectrx2020_deserialize_points(uint8_t* in_packed_points_buff, uint32_t cnt, bool color) {
	in_packed_points_buff += cnt;
	ds_dvg_vec point;
	static ds_colors_t prev_colors;

	point.pnt.rgb.r = 0;
	point.pnt.rgb.g = 0;
	point.pnt.rgb.b = 0;

	if (color) {

		prev_colors.r = point.pnt.rgb.r = *(in_packed_points_buff);
		prev_colors.g = point.pnt.rgb.g = *(in_packed_points_buff + 1);
		prev_colors.b = point.pnt.rgb.b = *(in_packed_points_buff + 2);
		cnt += 3;
		in_packed_points_buff += 3;
	}
	else {
		point.pnt.rgb.r = prev_colors.r;
		point.pnt.rgb.g = prev_colors.g;
		point.pnt.rgb.b = prev_colors.b;
	}

	point.pnt.y = ((*(in_packed_points_buff + 1) & 0xF0) << 4) + *(in_packed_points_buff);
	point.pnt.x = ((*(in_packed_points_buff + 2) & 0x0F) << 8) + ((*(in_packed_points_buff + 2) & 0xF0) >> 4) + ((*(in_packed_points_buff + 1) & 0x0F) << 4);
	cnt += 3;
	in_packed_points_buff += 3;

	//if(dctr<3)
	printf( "D line:%u color=%u x=%u y=%u r=%u g=%u b=%u\n", lineno++, color, point.pnt.x, point.pnt.y, point.pnt.rgb.r, point.pnt.rgb.g, point.pnt.rgb.b);
	
	return cnt;
}

 int32_t vectrx2020_deserialize(uint8_t* in_meta_buff, uint8_t* in_packed_points_buff) {

	uint8_t* start_points_buff = in_packed_points_buff;

	uint8_t meta_byte = 0;
	uint32_t int_bit_iter = 0, byte_iter = 0, in_p_size = 0;
	int32_t cnt = 0;
	printf("D FLAG_XY\n");
	ByteToint32(in_meta_buff + SIZEOF_HEADER, &in_p_size);
	if (in_p_size >= 8) {

		for (byte_iter = 0; byte_iter <= (in_p_size - 8); byte_iter += 8) {
			meta_byte = *(in_meta_buff + (SIZEOF_HEADER + SIZEOF_LEN) + byte_iter / 8);
			for (int_bit_iter = 0; int_bit_iter < 8; int_bit_iter++) {
				uint8_t colorbit = 0b1 & (meta_byte >> int_bit_iter);
				cnt = vectrx2020_deserialize_points(in_packed_points_buff, cnt, colorbit);
				if (cnt == -1)
					return -1;
			}
		}
	}
	if (in_p_size % 8) {

		meta_byte = *(in_meta_buff + (SIZEOF_HEADER + SIZEOF_LEN) + byte_iter / 8);

		for (int_bit_iter = 0; int_bit_iter < in_p_size % 8; int_bit_iter++) {
			uint8_t colorbit = 0b1 & (meta_byte >> int_bit_iter);
			cnt = vectrx2020_deserialize_points(in_packed_points_buff, cnt, colorbit);
			if (cnt == -1)
				return -1;
		}
	}
	in_packed_points_buff = start_points_buff;
	printf("D FLAG_COMPLETE\n");
	return cnt;
}


 int vector_device_udp_dvg::send_vectors()
 {

	 int      result = -1;

	 cmd_vec_postproc();

	 static uint64_t total_byte_ctr = 0;
	 static uint64_t chrono_byte_ctr = 0;
	 static uint64_t previous_byte_ctr = 0;

	 uint32_t out_bit_iter = 0;
	 uint8_t meta_byte = 0;
	 out_meta_points.clear();
	 out_m_packed_pnts.clear();
	 c_header[4] = FLAG_COMPLETE;
	 out_meta_points.assign(c_header, c_header + SIZEOF_HEADER);
 
	 dctr = 0;
 
	 bool first = true;

	 for (auto& vec : m_out_vectors)
	 {
		 dvg_vec point;
		 bool   blank;
 
		 int32_t y = vec.y1;
		 int32_t x = vec.x1;

		blank = (vec.color.r() == 0) && (vec.color.g() == 0) && (vec.color.b() == 0);

		
		 uint8_t m_ar[8];
		 transform_final(&vec.x1, &vec.y1);
		 point.pnt.x = (vec.x1);
		 point.pnt.y = (vec.y1);
		 int64ToByte(m_ar, point.val);
		 point.colors.color_change = (m_last_r != vec.color.r()) || (m_last_g != vec.color.g()) || (m_last_b != vec.color.b());
	//	 if (!blank)
	//	 {
			
			 if (point.colors.color_change) {
			
				 m_last_r = point.pnt.r = vec.color.r();
				 m_last_g = point.pnt.g = vec.color.g();
				 m_last_b = point.pnt.b = vec.color.b();

				 out_m_packed_pnts.push_back(m_ar[2]); //r
				 out_m_packed_pnts.push_back(m_ar[1]); //g
				 out_m_packed_pnts.push_back(m_ar[0]); //b
			 }
	//	 }
		


		 if (!(out_bit_iter % 8) && out_bit_iter) {
			 out_meta_points.push_back(meta_byte);
			 meta_byte = 0;
		 }

		 meta_byte |= point.colors.color_change << (out_bit_iter % 8);
		 out_m_packed_pnts.push_back(m_ar[4]); //xy (backwards)
		 out_m_packed_pnts.push_back(((m_ar[5] & 0x0F) << 4) | ((m_ar[6] & 0xF0) >> 4));
		 out_m_packed_pnts.push_back(((m_ar[6] & 0x0F) << 4) | (m_ar[7] & 0x0F));
		 out_bit_iter++;

#if MAME_DEBUG2
		 printf("W line:%llu color_change=%u x=%u y=%u r=%u g=%u b=%u \n\r", lineno++, point.colors.color_change, point.pnt.x, point.pnt.y, point.pnt.r, point.pnt.g, point.pnt.b);
#endif
	 }
uint32_t out_points_size = m_out_vectors.size();
static uint32_t frame_counter;
	out_meta_points.push_back(meta_byte);
	uint8_t meta_out_points_size[4];
	int32ToByte(meta_out_points_size, out_points_size);

	for (int i = 3; i >= 0; i--)
		out_meta_points.insert(out_meta_points.begin() + SIZEOF_HEADER, meta_out_points_size[i]);


	uint8_t* out_points_buff = out_m_packed_pnts.data();
	int32_t full_buffers = 0;
	int32_t out_points_packed_size = out_m_packed_pnts.size();
	static uint32_t point_bytes = 0;
	c_header[4] = FLAG_XY;
#if MAME_DEBUG

		printf("I FLAG_XY\n");

#endif
	result = packets_write((uint8_t*)c_header, SIZEOF_HEADER);
	

	for (full_buffers = 0; full_buffers < (out_points_packed_size - BUFF_SIZE); full_buffers += BUFF_SIZE) {
		memcpy((char*)m_send_buffer, (char*)out_points_buff + full_buffers, BUFF_SIZE);
		point_bytes += BUFF_SIZE;

		result = packets_write(m_send_buffer, BUFF_SIZE );
	}
	
	if (out_points_packed_size - full_buffers)
	{
		memcpy((char*)m_send_buffer, (char*)out_points_buff + full_buffers, out_points_packed_size % BUFF_SIZE);
		result = packets_write(m_send_buffer, out_points_packed_size % BUFF_SIZE);
		point_bytes += out_points_packed_size % BUFF_SIZE;
	}
	
//	total_byte_ctr += out_meta_points.size() + out_points_packed_size + 6;
	//chrono_byte_ctr += out_meta_points.size() + out_points_packed_size + 6;
	
	total_byte_ctr +=  out_points_packed_size ;
	chrono_byte_ctr += out_points_packed_size ;
	static std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<__int64, std::ratio<1, 1000000000>>>	timer_start;
	if (chrono_byte_ctr >= ( 1000 * BUFF_SIZE)) {
		auto timer_elapsed = high_resolution_clock::now() - timer_start;
		long long ms = std::chrono::duration_cast<microseconds>(timer_elapsed).count();
		 rcvMBps.f = (float)chrono_byte_ctr / (float)ms;
		 
		timer_start = high_resolution_clock::now();
		chrono_byte_ctr = 0;
	 
	}
	int ci = 0;
	  dctr = 0;
#if MAME_DEBUG

	  for (auto& vec : m_out_vectors)
	  {
		  dvg_vec point;
		  bool   blank;

	 
		  blank = (vec.color.r() == 0) && (vec.color.g() == 0) && (vec.color.b() == 0);

		
		  transform_final(&vec.x1, &vec.y1);
		  point.pnt.x = (vec.x1);
		  point.pnt.y = (vec.y1);
		  uint8_t m_ar[8];
		  point.colors.color_change = (m_last_r != vec.color.r()) || (m_last_g != vec.color.g()) || (m_last_b != vec.color.b());
		  if (!blank)
		  {
			
			  if (point.colors.color_change) {
				  m_last_r = vec.color.r();
				  m_last_g = vec.color.g();
				  m_last_b = vec.color.b();
			  }
			  point.pnt.r = m_last_r;
			  point.pnt.g = m_last_g;
			  point.pnt.b = m_last_b;


		  }
		 

		printf("I line:%llu color_change=%u x=%u y=%u r=%u g=%u b=%u \n\r", lineno2++, point.colors.color_change, point.pnt.x, point.pnt.y, point.pnt.r, point.pnt.g, point.pnt.b);
		dctr++;
	}
#endif
#if MAME_DEBUG
	
		printf("I FLAG_COMPLETE\n");
	
#endif
		 
	result = packets_write(out_meta_points.data(), out_meta_points.size());
	cmd_reset(0);
	m_last_r = m_last_g = m_last_b = -1;

	frame_counter += 1;
	if (!(frame_counter % 100)) {
	//	printf("%lu frames @ %llu bytes per frame || ", frame_counter, (total_byte_ctr - previous_byte_ctr) / 100);
	//	printf("total bytes: %lu upstream @ %09.4f MBps (0x%08lx)\n\r", point_bytes, rcvMBps.f, rcvMBps.u);
		

		previous_byte_ctr = total_byte_ctr;
	}

#if MAME_DEBUG
	vectrx2020_deserialize(out_meta_points.data(), out_m_packed_pnts.data());
#endif
	return result;
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

void vector_device_udp_dvg::get_dvg_info()
{
	int      result;
	uint32_t version, major, minor;

	if (m_json_length)
	{
		return;
	}


	sscanf(emulator_info::get_build_version(), "%u.%u", &major, &minor);
	version = (((minor / 1000) % 10) << 12) | (((minor / 100) % 10) << 8) | (((minor / 10) % 10) << 4) | (minor % 10);
	sprintf((char*)m_send_buffer, "$cmd%c %lu", FLAG_GET_DVG_INFO, version);
	packets_write(m_send_buffer, CMD_LEN + 4);

	result = packet_read(reinterpret_cast<uint8_t*> (&m_recv_buffer), 2);
	uint16_t len = (m_recv_buffer[0] << 8) + m_recv_buffer[1];
	result = packet_read(reinterpret_cast<uint8_t*> (&m_recv_buffer), len);

	StringBuffer sb;
	PrettyWriter writer(sb);
	GameDetails(machine()).Serialize(writer);

	const char* json = sb.GetString();
	uint16_t json_len = (uint16_t)sb.GetSize();


	sprintf((char*)m_send_buffer, "$cmd%c %s", FLAG_GET_GAME_INFO, json);
	packets_write(m_send_buffer, CMD_LEN + json_len);


}
uint32_t vector_device_udp_dvg::screen_update(screen_device& screen, bitmap_rgb32& bitmap, const rectangle& cliprect)
{
	rgb_t color = rgb_t(108, 108, 108);
	rgb_t black = rgb_t(0, 0, 0);
	int x0, y0, x1, y1;

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
