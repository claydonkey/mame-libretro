// license:BSD-3-Clause
// copyright-holders:Mario Montminy, Anthony Campbell
#ifndef VECTOR_UDP_DVG_H
#define VECTOR_UDP_DVG_H
#pragma once

#include "osdcore.h"
#include "screen.h"
#include "divector.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <vector>
#include "msgpack/msgpack.hpp"
#define MAX_JSON_SIZE           512
#define BUFF_SIZE                  1024

#ifdef __GNUC__
#define PACK( __Declaration__ ) __Declaration__ __attribute__((__packed__))
#endif

#ifdef _MSC_VER
#define PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop))
#endif

struct dvg_rgb_t
{
	uint8_t r;
	uint8_t g;
	uint8_t b;
	template<class T>
	void pack(T& pack) {
		pack( r, g, b);
	}
};
struct  dvg_point_t
{

	uint16_t x;
	uint16_t y;
	dvg_rgb_t color;

	template<class T>
	void pack(T& pack) {
		pack(x, y, color);
	}
};

struct dvg_points_t {
	std::vector< dvg_point_t> pnt;
	uint16_t point_count;
	template<class T>
	void pack(T& pack) {
		pack(pnt, point_count);
	}
};
 
class vector_device_udp_dvg : public vector_interface
{

public:


	typedef struct vec_t
	{

		int32_t x0;
		int32_t y0;
		int32_t x1;
		int32_t y1;
		rgb_t color;
		bool screen_coords;
	} vector_t;

	typedef struct
	{
		const char* name;
		bool exclude_blank_vectors;
		uint32_t artwork;
		bool bw_game;
	} game_info_t;

	vector_device_udp_dvg(const machine_config& mconfig, const char* tag, device_t* owner, uint32_t clock = 0);

	// device-level overrides

	virtual void add_point(int x, int y, rgb_t color, int intensity) override;
	virtual uint32_t screen_update(screen_device& screen, bitmap_rgb32& bitmap, const rectangle& cliprect) override;

private:

	 
	uint32_t compute_code(int32_t x, int32_t y);
	uint32_t line_clip(int32_t* pX1, int32_t* pY1, int32_t* pX2, int32_t* pY2);
	void cmd_vec_postproc();
	void cmd_reset(uint32_t initial);
	void cmd_add_vec(int x, int y, rgb_t color, bool screen_coords);

	void get_dvg_info();
	int packet_read(uint8_t* buf, int size);
	int packets_write(uint8_t* buf, int size);
	int send_vectors();
	void transform_and_scale_coords(int* px, int* py);
	int determine_game_settings();
	void transform_final(int* px, int* py);

	static const game_info_t s_games[];
	bool m_exclude_blank_vectors;
	int32_t m_xmin;
	int32_t m_xmax;
	int32_t m_ymin;
	int32_t m_ymax;
	float m_xscale;
	float m_yscale;
	uint8_t m_send_buffer[BUFF_SIZE];
	uint8_t m_recv_buffer[BUFF_SIZE];
	bool m_swap_xy;
	bool m_flip_x;
	bool m_flip_y;
	int32_t m_clipx_min;
	int32_t m_clipx_max;
	int32_t m_clipy_min;
	int32_t m_clipy_max;
	int32_t m_last_r;
	int32_t m_last_g;
	int32_t m_last_b;
	uint32_t m_artwork;
	bool m_bw_game;
	std::vector<vector_t> m_in_vectors;
	std::vector<vector_t> m_out_vectors;
	uint32_t m_in_vec_last_x;
	uint32_t m_in_vec_last_y;
 

	uint32_t m_vertical_display;
	osd_file::ptr m_port;

	std::string m_host;
	WSADATA m_wsa;
	char m_message[BUFF_SIZE];
	//uint8_t  m_json_buf[MAX_JSON_SIZE];
	int m_json_length;
	std::unique_ptr<uint8_t[]> m_json_buf;
	std::unique_ptr<float[]> m_gamma_table;
	void device_sethost(std::string _host);
	std::error_condition  device_start_client();
	void device_stop_client();
protected:
	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void device_stop() override;

};

// device type definition
DECLARE_DEVICE_TYPE(VECTOR_UDP_DVG, vector_device_udp_dvg)
#endif // VECTOR_USB_DVG_H
