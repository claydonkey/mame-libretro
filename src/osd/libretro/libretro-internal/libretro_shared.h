#ifndef _LIBRETRO_SHARED_H
#define _LIBRETRO_SHARED_H

#include "libretro.h"

#ifndef RETRO_MAX_BUTTONS
#define RETRO_MAX_BUTTONS 16
#endif

#define HAVE_RGB32
//FIXME: re-add way to handle 16/32 bit
#if (!defined(HAVE_OPENGL) && !defined(HAVE_RGB32)) || (!defined(HAVE_OPENGLES) && !defined(HAVE_RGB32))

#ifndef M16B
#define M16B
#endif

#endif

#ifndef M16B
#define PIXEL_TYPE UINT32
#else
#define PIXEL_TYPE UINT16
#endif

enum
{
   RETROPAD_B,
   RETROPAD_Y,
   RETROPAD_SELECT,
   RETROPAD_START,
   RETROPAD_PAD_UP,
   RETROPAD_PAD_DOWN,
   RETROPAD_PAD_LEFT,
   RETROPAD_PAD_RIGHT,
   RETROPAD_A,
   RETROPAD_X,
   RETROPAD_L,
   RETROPAD_R,
   RETROPAD_L2,
   RETROPAD_R2,
   RETROPAD_L3,
   RETROPAD_R3,
   RETROPAD_TOTAL
};

enum
{
   RETRO_SETTING_LIGHTGUN_MODE_DISABLED,
   RETRO_SETTING_LIGHTGUN_MODE_POINTER,
   RETRO_SETTING_LIGHTGUN_MODE_LIGHTGUN
};

enum
{
   RETRO_SETTING_VECTOR_DRIVER_SCREEN,
   RETRO_SETTING_VECTOR_DRIVER_USB_DVG,
   RETRO_SETTING_VECTOR_DRIVER_V_ST
};
extern int NEWGAME_FROM_OSD;

extern char g_rom_dir[1024];
extern const char *retro_save_directory;
extern const char *retro_system_directory;
extern const char *retro_content_directory;
extern int retro_pause;

extern bool experimental_cmdline;
extern bool hide_gameinfo;
extern bool mouse_enable;
extern int  lightgun_mode;
extern bool cheats_enable;
extern bool alternate_renderer;
extern bool boot_to_osd_enable;
extern bool boot_to_bios_enable;
extern bool softlist_enable;
extern bool softlist_auto;
extern bool write_config_enable;
extern bool read_config_enable;
extern bool hide_nagscreen;
extern bool hide_warnings;
extern bool throttle_enable;
extern bool auto_save_enable;
extern bool game_specific_saves_enable;
extern bool buttons_profiles;
extern bool mame_paths_enable;
extern bool mame_4way_enable;
extern char mame_4way_map[256];
extern int  vector_driver;
extern char vector_port[20];
extern bool vector_screen_mirror;
extern float vector_scale;
extern float vector_scale_x;
extern float vector_scale_y;
extern float vector_offset_x;
extern float vector_offset_y;
extern int  vector_rotate;
extern int  vector_bright;
extern bool res_43;
extern bool video_changed;

extern int mouseLX[8];
extern int mouseLY[8];
extern int mouseBUT[4];

extern int lightgunX[8];
extern int lightgunY[8];
extern int lightgunBUT[4];

extern unsigned short retrokbd_state[RETROK_LAST];

extern char mediaType[10];

extern bool nobuffer_enable;

extern int mame_reset;

extern int ui_ipt_pushchar;

extern int fb_width;
extern int fb_height;
extern int fb_pitch;
extern int max_width;
extern int max_height;
extern float retro_aspect;
extern float retro_fps;
extern float view_aspect;
static const char core[] = "mame";

/* libretro callbacks */
extern retro_log_printf_t log_cb;
extern retro_environment_t environ_cb;
extern retro_input_state_t input_state_cb;
extern retro_input_poll_t input_poll_cb;

void retro_switch_to_main_thread(void);

void retro_frame_draw_enable(bool enable);

void *retro_get_fb_ptr(void);

#ifdef __cplusplus
extern "C" {
#endif

int mmain2(int argc, const char *argv);
int mmain(int argc, char *argv[]);

#ifdef __cplusplus
}
#endif


#endif
