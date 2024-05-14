#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
#define DOUBLE_BUFFER_DEFAULT		0
#define SINGLE_BUFFER_DEFAULT		1
#define SINGLE_BUFFER_FIXED_DEFAULT	2

	typedef struct _textpos_t {
		uint16_t y;
		uint16_t x;
	} textpos_t;

	typedef struct _m_game_settings_t game_settings_t;
	typedef enum _m_orientation_enum {
		DEFAULT_ORIENTATION = 0,
		ROT_90,
		ROT_180,
		ROT_270,
		FLIP_X,
		FLIP_Y,
		FLIP_XY
	} m_orientation_enum;

	typedef enum _enum_buffer_type_t {
		DOUBLE_BUFFER = DOUBLE_BUFFER_DEFAULT,
		SINGLE_BUFFER = SINGLE_BUFFER_DEFAULT,
		SINGLE_BUFFER_FIXED = SINGLE_BUFFER_FIXED_DEFAULT
	} enum_buffer_type_t;



	typedef enum _enum_buffer_mode_t {
		DMA_NORMAL_MODE = ((uint32_t)0x00000000U), /*!< Normal mode                                    */
		DMA_CIRCULAR_MODE , /*!< Circular mode                                  */
		DMA_PFCTRL_MODE  , /*!< Peripheral flow control mode                   */
		DMA_DOUBLE_BUFFER_M0_MODE , /*!< Double buffer mode with first target memory M0 */
		DMA_DOUBLE_BUFFER_M1_MODE  /*!< Double buffer mode with first target memory M1 */
	} enum_buffer_mode_t;

	typedef enum {
		PLAY_MODE = 0,
		TEST_MODE = 1 << 1,
		FREE_PLAY = 1 << 2
	} m_arcade_mode;

	typedef struct _m_game_settings_t {

		const char* name;
		uint16_t dac_period;
		uint16_t draw_steps;
		uint16_t dwell;
		bool fine_type;
		bool controllers;
		bool lcd;
		bool audio;
		bool draw_jumps;
		bool auto_sync;
		enum_buffer_mode_t buffer_mode;
		enum_buffer_type_t buffer_type;

		uint16_t scalex;
		uint16_t scaley;
		m_orientation_enum orientation;
		uint32_t ui_flags;


		uint32_t load_audio_len;
		m_arcade_mode arcade_mode;
	} m_game_settings_t;

	typedef enum _m_mode_enum {
		NO_MODE = 0,
		M_MAME_WIFI,
		M_V3D,
		M_EMU,
		M_PAGES,
		M_OPENGL,
		M_MUSIC,
		M_ILDA,
		M_UI,
		M_SVG,
		M_CTRLS,
		M_DVG,
		M_ZIP,
		M_SYS
	} m_mode_enum;

	typedef enum _m_ui_flags {
		mUI_OFF = 0,
		mSPINNER = 1 << 1,
		mUART_DEBUG = 1 << 2,
		mCRT_OVERLAY = 1 << 3,
		mLCD_OVERLAY = 1 << 4,
		mLCD_MIRROR = 1 << 5,
		mSCREEN = 1 << 6,
		mTIMINGS = 1 << 7,
		mHAPTICS = 1 << 8,
		mGEOM = 1 << 9,
		mMODE = 1 << 10,
		mMEM = 1 << 11,
		mLOGO = 1 << 12,
		mFPS = 1 << 13,
		mSWITCHES = 1 << 14,
		mFFT = 1 << 15,
		mFRAME = 1 << 16,
		mOVERFLOW = 1 << 17,
		mPOINTS = 1 << 18,
		mFRAME_CPLXTY = 1 << 19,
	    mCONNECTED = 1 << 20,
		mEMPTY = 1 << 21
	} m_ui_flags;

#define DOUBLE_BUFFER_DEFAULT		0
#define SINGLE_BUFFER_DEFAULT		1
#define SINGLE_BUFFER_FIXED_DEFAULT	2


	typedef enum _m_pagetype_enum {
		PAGE_OFF = 0,
		ANALOG_CLOCK,
		DIGITAL_CLOCK,
		ALPHABET,
		LOREM,
		SVG_TEST,
		END
	} m_pagetype_enum;

	typedef enum
	{
		PROTO_UDP = 0,
		PROTO_TCP = 1

	} m_protocol_enum_t;

	typedef struct _m_settings_t {
		uint16_t dac_period;
		uint16_t draw_steps;
		uint16_t dwell;
		bool fine_type;
		bool controllers;
		bool lcd;
		bool audio;
		bool draw_jumps;
		bool auto_sync;
		enum_buffer_mode_t buffer_mode;
		enum_buffer_type_t buffer_type;
		uint16_t scalex;
		uint16_t scaley;
		m_orientation_enum orientation;
		uint32_t ui_flags;
		uint16_t romsize;
		uint16_t disp_tri_num;
		uint16_t disp_seg_num;
		uint16_t stl_tri_num;
		uint32_t stl_tri_mem;
		uint32_t frame_ctr;
		uint16_t app_dac_period;
		uint16_t app_draw_steps;
		uint16_t minx;
		uint16_t miny;
		uint16_t maxx;
		uint16_t maxy;
		bool in_freertos;
		m_protocol_enum_t protocol;
		uint8_t compressionLevel;
		bool rgb;
		uint16_t move_steps;
		bool stats;
		uint16_t x_offset;
		uint16_t y_offset;
	} m_settings_t;

	typedef struct _p_timer_t {
		uint32_t stamp;
		uint32_t ptime;
		uint32_t time;
		const char* name;
	} p_timer_t;

	typedef enum _p_state_enum_t {
		UI_LOADING = 1 << 0,
		DISP_LCD = 1 << 1,
		PAUSE_MAIN = 1 << 2,
		USB_RUNNING = 1 << 3,
		SCREEN_ON = 1 << 4,
		RENDERING_DVG = 1 << 5,
		PAUSE_DVG = 1 << 6

	} p_state_enum_t;

	typedef struct _p_fps_t {
		double fps;
		uint32_t mstart;
		uint32_t mend;
	} p_fps_t;

	typedef enum _m_font_t {
		HERSHEY_FONT, ASTEROIDS_FONT
	} m_font_t;



	typedef struct _m_stats_t {
		p_fps_t fps_timer;
		bool overflowed;
		double ink;
		double no_ink;
		p_timer_t p_timer[40];
		uint32_t frame_complexity;
	} m_stats_t;


	typedef struct _m_st_t {
		const char* name;
		m_mode_enum mode;
		m_settings_t settings;
		uint16_t timer_idx;
		m_stats_t stats;
		float spec_scale;
		uint16_t buffer_size_override;;
		m_font_t font_type;
		m_pagetype_enum page;


	} m_st_t;
#ifdef __cplusplus
}
#endif
