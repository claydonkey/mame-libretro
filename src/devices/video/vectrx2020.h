// license:BSD-3-Clause
// copyright-holders:Mario Montminy, Anthony Campbell
#ifndef VECTRX2020_H
#define VECTRX2020_H
#pragma once

#include "osdcore.h"
#include "screen.h"
#include "video/vector.h"
#include "vectrx2020_ui.h"
#include <WindowsDeviceHandler.h>

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

class GameDetails {
public:
	GameDetails(running_machine& machine);
	GameDetails(const GameDetails& rhs);
	~GameDetails();
	template <typename Writer>
	void Serialize(Writer& writer) const;

private:

	//-------------------------------------------------
	//  get_screen_desc - returns the description for
	//  a given screen
	//-------------------------------------------------
	std::string get_screen_desc(screen_device& screen) const;
	running_machine& machine_;
	std::string fullname_;
	std::string name_;
	std::string manufacturer_;
	std::string rom_;
	std::string year_;
	std::string video_;
	std::string m_cpu_;
	std::string driver_;
	std::string sound_;
	machine_flags::type flags_;

};



using namespace std;
using namespace vectrx2020;
class vectrx2020_device : public vector_interface {
public:


	typedef struct {
		const char* name;
		bool exclude_blank_vectors;
		uint32_t artwork;
		bool bw_game;
	} game_info_t;

	vectrx2020_device(const machine_config& mconfig, const char* tag, device_t* owner, uint32_t clock = 0);
	virtual void add_point(int x, int y, ::rgb_t color, int intensity) override;
	virtual uint32_t screen_update(screen_device& screen, bitmap_rgb32& bitmap, const rectangle& cliprect) override;
	bool begin_segment_frame(screen_device &screen);
	void add_segment(int x0, int y0, int x1, int y1, ::rgb_t color);
	void end_segment_frame();
	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void device_stop() override;
	// ---- getters ----
	int                     get_dwell() const;
	m_sync_mode_t           get_sync_mode() const;
	int                     get_draw_steps() const;
	m_compression_level_t   get_compression() const;
	int                     get_dac_period() const;

	int                     get_scale_x() const;
	int                     get_scale_y() const;
	int                     get_x_offset() const;
	int                     get_y_offset() const;

	m_orientation_enum      get_orientation() const;
	bool                    get_rgb() const;

	// ---- setters ----
	void set_dwell(int dwell);
	void set_sync_mode(m_sync_mode_t mode);
	void set_draw_steps(int steps);
	void set_compression(m_compression_level_t level);
	void set_dac_period(int period);
	void set_scale_x(int percent);
	void set_scale_y(int percent);
	void set_offset_x(int offset);
	void set_offset_y(int offset);
	void set_orientation(m_orientation_enum orient);
	void set_rgb(bool enable);
	void set_vector_host(const char* host);

private:
	bool m_exclude_blank_vectors;
	int32_t m_xmin;
	int32_t m_xmax;
	int32_t m_ymin;
	int32_t m_ymax;
	float m_xscale;
	float m_yscale;
	bool m_swap_xy;
	bool m_flip_x;
	bool m_flip_y;
	uint32_t m_artwork;
	bool m_bw_game;
	uint32_t m_vertical_display;
	static const game_info_t s_games[];
	std::vector<dvg_pnt> m_points;
	m_st_t m_st;
	bool m_first_point = false;
	bool m_segment_ready = false;
	bool m_segment_warning = false;
	unsigned m_segment_points = 0;
	unsigned m_segment_frames = 0;
	std::chrono::steady_clock::time_point m_next_segment_frame{};
	std::unique_ptr<vectrx2020::WindowsDeviceHandler> m_vector_device;
	bool m_centre_beam;
	std::string  m_host;

	virtual bool send_vectors();
	void transform_final(int* px, int* py);
	uint32_t compute_code(int32_t x, int32_t y);
	uint32_t line_clip(int32_t* pX1, int32_t* pY1, int32_t* pX2, int32_t* pY2);
 
	void transform_and_scale_coords(int* px, int* py);
	bool send_local_info();
	bool get_dvg_info();
	bool set_dvg_settings();
	bool get_dvg_settings();
 


protected:
	int32_t m_clipx_min;
	int32_t m_clipx_max;
	int32_t m_clipy_min;
	int32_t m_clipy_max;
	std::unique_ptr<float[] > m_gamma_table;

};

// device type definition
DECLARE_DEVICE_TYPE(VECTRX2020, vectrx2020_device)
#endif 
