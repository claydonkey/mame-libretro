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


#ifdef __GNUC__
#define PACK( __Declaration__ ) __Declaration__ __attribute__((__packed__))
#endif

#ifdef _MSC_VER
#define PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop))
#endif



#ifdef __GNUC__
#define PACK( __Declaration__ ) __Declaration__ __attribute__((__packed__))
#endif

#ifdef _MSC_VER
#define PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop))
#endif

#define MAX_JSON_SIZE           512
 
#define DVG_RELEASE             0
#define DVG_BUILD               1
#define CMD_BUF_SIZE            0x20000
#define DVG_RES_MIN              0
#define DVG_RES_MAX              4095
#define SAVE_TO_FILE             0
#define SORT_VECTORS             0
#define MAX_VECTORS              0x10000
#define MAX_JSON_SIZE            512
#define GAME_NONE                0
#define GAME_ARMORA              1
#define GAME_WARRIOR             2

#define MAX_JSON_SIZE           512
#define BUFF_SIZE               1470U
#define SIZEOF_LEN 4
#define SIZEOF_HEADER 9
#define SIZEOF_PKT_CTR 2
#define SERVER_BUFF_SIZE (1470U)
#define SERVER_DEBUG_TX_BYTES 100000
#define EOT_NEW             TRUE

#define SERVER_DATA_SIZE (SERVER_BUFF_SIZE - SIZEOF_HEADER)
#define SIZEOF_CHECKSUM sizeof(uint32_t)


uint64_t ByteToint64(uint8_t a[], uint64_t* n);
uint32_t ByteToint32(uint8_t a[], uint32_t* n);
uint16_t ByteToint16(uint8_t a[], uint16_t* n);
uint8_t* int64ToByte(uint8_t a[], uint64_t n);
uint8_t * int32ToByte(uint8_t a[], uint32_t n);
uint8_t* int16ToByte(uint8_t a[], uint16_t n);
typedef enum _cmd_enum
{

	FLAG_RGB = 0x1,
	FLAG_DVG_POINTS = 0x2,
	FLAG_GAME = 0x3,
	FLAG_META = 0x4,
	FLAG_POINT = 0xB,
	FLAG_CMD = 0x5,
	FLAG_CMD_END = 0x6,
	FLAG_EXIT = 0x7,
	FLAG_GET_DVG_INFO = 0x8,
	FLAG_GET_GAME_INFO = 0x9,
	FLAG_SET_PROTOCOL = 0xB,
	FLAG_META_EOT = 0xC,
	FLAG_EOT = 0xE,
	FLAG_COMPLETE_MONOCHROME = 0xA
}cmd_enum;

using namespace std;

typedef enum
{
	LEVEL_NONE = 0,
	LEVEL_SAFE,
	LEVEL_1,
	LEVEL_2,
	LEVEL_3,
	LEVEL_4,
	LEVEL_5,
	LEVEL_6,
	LEVEL_7,
	LEVEL_8,
	LEVEL_9,
	LEVEL_10,
	LEVEL_11,
	LEVEL_12,
	LEVEL_13,
	LEVEL_14,
	LEVEL_15,
	LEVEL_MAX
} m_compression_level_t;

 

PACK(struct point_t {

	uint16_t b : 8;
	uint16_t g : 8;
	uint16_t r : 8;
	uint16_t y : 12;
	uint16_t x : 12;
});



PACK(struct v_colors_t {
	uint32_t rgb : 24;
	uint32_t yx : 24;

});

union dvg_vec {
	point_t pnt;
	uint64_t val;
	 
};

typedef union _ser_float {
	float f;
	uint32_t u;
} ser_float;


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

	typedef enum
	{
		NO_HEADER=0,
		HEADER_1=1,
		HEADER_2=2
	}xy_header_enum_t;
	typedef enum
	{
		PROTO_UDP = 0,
		PROTO_TCP = 1
	
	}protocol_enum_t;
	typedef struct
	{
		const char* name;
		bool exclude_blank_vectors;
		uint32_t artwork;
		bool bw_game;
	} game_info_t;

	vector_device_udp_dvg(const machine_config& mconfig, const char* tag, device_t* owner, uint32_t clock = 0);

	// device-level overrides
	int32_t deserialize();
	virtual void add_point(int x, int y, rgb_t color, int intensity) override;
	virtual uint32_t screen_update(screen_device& screen, bitmap_rgb32& bitmap, const rectangle& cliprect) override;

private:

	uint32_t out_bit_iter ;
	uint8_t meta_byte ;
	 uint64_t total_byte_ctr;
	 uint64_t chrono_byte_ctr ;
	 uint64_t previous_byte_ctr = 0;

	 void sendGameInfo();
	int32_t deserialize_points(uint8_t* in_packed_points_buff, uint32_t cnt, bool color_change);

	 
	protocol_enum_t m_protocol;
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

	int32_t m_last_r;
	int32_t m_last_g;
	int32_t m_last_b;
	int32_t m_last_r2;
	int32_t m_last_g2;
	int32_t m_last_b2;

	uint32_t m_artwork;
	bool m_bw_game;
	uint32_t m_vertical_display;
	osd_file::ptr m_port;

	std::string m_host;
	WSADATA m_wsa;
	char m_message[BUFF_SIZE];
 

	static const game_info_t s_games[];
	std::vector<uint8_t> out_meta;
	std::vector<uint8_t> out_data;
	std::vector<vector_t> m_in_vectors;
	std::vector<vector_t> m_out_vectors;
	uint32_t m_in_vec_last_x;
	uint32_t m_in_vec_last_y;
	void get_dvg_info();
	void set_dvg_protocol();
	//int packet_read(uint8_t* buf, int size);
	//int packets_write(uint8_t* buf, int size);
	virtual int send_vectors();
	bool compressData(const std::string& input_data, std::string& compressed_data, m_compression_level_t compression_level);
	int sendFrame(const std::string& meta_data, const std::string& point_data);
	void transform_final(int* px, int* py);
	std::vector<uint8_t> out_meta_points;
	std::vector<uint8_t> out_m_packed_pnts;
	uint32_t compute_code(int32_t x, int32_t y);
	uint32_t line_clip(int32_t* pX1, int32_t* pY1, int32_t* pX2, int32_t* pY2);
	void cmd_vec_postproc();
	void cmd_reset(uint32_t initial);
	void cmd_add_vec(int x, int y, rgb_t color, bool screen_coords);
	std::size_t readPacket(const cmd_enum command, uint8_t* message);
	std::size_t writePacket(const cmd_enum command, const char* data, std::size_t data_size);
	void transform_and_scale_coords(int* px, int* py);
	std::size_t sendEOT(std::size_t data_size, cmd_enum eottype);
	

protected:
	virtual std::error_condition  device_start_client();
	virtual void device_stop_client();
	void device_sethost(std::string _host);
	int32_t m_clipx_min;
	int32_t m_clipx_max;
	int32_t m_clipy_min;
	int32_t m_clipy_max;
	std::unique_ptr<float[]> m_gamma_table;
	int m_json_length;
	ser_float rcvMBps;
	std::unique_ptr<uint8_t[]> m_json_buf;
	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void device_stop() override;
	int determine_game_settings();
};

// device type definition
DECLARE_DEVICE_TYPE(VECTOR_UDP_DVG, vector_device_udp_dvg)
#endif // VECTOR_USB_DVG_H
