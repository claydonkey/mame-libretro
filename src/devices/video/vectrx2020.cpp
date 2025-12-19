// license:BSD-3-Clause
// copyright-holders: Anthony Campbell
#include "emu.h"
#include "vector.h"
#include "vectrx2020.h"
#include "emuopts.h"
#include "rendutil.h"

#include <rapidjson/include/rapidjson/document.h>
#include <rapidjson/include/rapidjson/prettywriter.h>
#include <WindowsDeviceHandler.h>
#include "vectrx2020_ui.h"

#include "main.h"
#include <language.h>
#include <drivenum.h>
#include <cassert>
#include <chrono>
#include <thread>
#include <algorithm>
#include <iostream>
#include <functional>


using namespace std::chrono;
using namespace std::this_thread; // sleep_for, sleep_until
using namespace std::chrono_literals; // ns, us, ms, s, h, etc.
using namespace util;
using namespace std; // Defining region codes 




#define DVG_RES_MIN              0
#define DVG_RES_MAX              4095

#define SAVE_TO_FILE             0
#define SORT_VECTORS             0
#define MAX_VECTORS              0x10000
#define MAX_JSON_SIZE            512

// Defining region codes 
#define LEFT                     0x1
#define RIGHT                    0x2
#define BOTTOM                   0x4
#define TOP                      0x8

#define GAME_NONE                0
#define GAME_ARMORA              1
#define GAME_WARRIOR             2

// these are UTF-8 encoded strings for common characters
#define UTF8_NBSP               "\xc2\xa0"          /* non-breaking space */

#define UTF8_MULTIPLY           "\xc3\x97"          /* multiplication sign */
#define UTF8_DIVIDE             "\xc3\xb7"          /* division sign */
#define UTF8_SQUAREROOT         "\xe2\x88\x9a"      /* square root symbol */
#define UTF8_PLUSMINUS          "\xc2\xb1"          /* plusminus symbol */

#define UTF8_POW_2              "\xc2\xb2"          /* superscript 2 */
#define UTF8_POW_X              "\xcb\xa3"          /* superscript x */
#define UTF8_POW_Y              "\xca\xb8"          /* superscript y */
#define UTF8_PRIME              "\xca\xb9"          /* prime symbol */
#define UTF8_DEGREES            "\xc2\xb0"          /* degrees symbol */

#define UTF8_SMALL_PI           "\xcf\x80"          /* Greek small letter pi */
#define UTF8_CAPITAL_SIGMA      "\xce\xa3"          /* Greek capital letter sigma */
#define UTF8_CAPITAL_DELTA      "\xce\x94"          /* Greek capital letter delta */

#define UTF8_MACRON             "\xc2\xaf"          /* macron symbol */
#define UTF8_NONSPACE_MACRON    "\xcc\x84"          /* nonspace macron, use after another char */

#define a_RING                  "\xc3\xa5"          /* small a with a ring */
#define a_UMLAUT                "\xc3\xa4"          /* small a with an umlaut */
#define o_UMLAUT                "\xc3\xb6"          /* small o with an umlaut */
#define u_UMLAUT                "\xc3\xbc"          /* small u with an umlaut */
#define e_ACUTE                 "\xc3\xa9"          /* small e with an acute */
#define n_TILDE                 "\xc3\xb1"          /* small n with a tilde */

#define A_RING                  "\xc3\x85"          /* capital A with a ring */
#define A_UMLAUT                "\xc3\x84"          /* capital A with an umlaut */
#define O_UMLAUT                "\xc3\x96"          /* capital O with an umlaut */
#define U_UMLAUT                "\xc3\x9c"          /* capital U with an umlaut */
#define E_ACUTE                 "\xc3\x89"          /* capital E with an acute */
#define N_TILDE                 "\xc3\x91"          /* capital N with a tilde */

#define UTF8_LEFT               "\xe2\x86\x90"      /* cursor left */
#define UTF8_RIGHT              "\xe2\x86\x92"      /* cursor right */
#define UTF8_UP                 "\xe2\x86\x91"      /* cursor up */
#define UTF8_DOWN               "\xe2\x86\x93"      /* cursor down */
DEFINE_DEVICE_TYPE(VECTRX2020, vectrx2020_device, "vectrx2020", "VECTRX2020")

using namespace rapidjson;

GameDetails::GameDetails(running_machine& machine) : machine_(machine) {
	std::unordered_set<std::string> exectags;
	execute_interface_enumerator execiter(machine.root_device());
	std::ostringstream cpu_str_buf;
	std::ostringstream driver_buf;
	std::ostringstream sound_buf;
	std::ostringstream video_buf;
	// print description, manufacturer, and CPU:
	std::string_view src(machine.system().type.source());
	auto prefix(src.find("src/mame/"));
	if (std::string_view::npos == prefix)
		prefix = src.find("src\\mame\\");
	if (std::string_view::npos != prefix)
		src.remove_prefix(prefix + 9);
	util::stream_format(driver_buf, _("%1$s %2$s Driver: %3$s"),
		//ui::system_list::instance().systems()[driver_list::find(machine().system().name)].description,
		machine.system().year,
		machine.system().manufacturer,
		src);


	for (device_execute_interface& exec : execiter) {
		if (!exectags.insert(exec.device().tag()).second)
			continue;
		// get cpu specific clock that takes internal multiplier/dividers in to account
		u32 clock = exec.device().clock();

		// count how many identical CPUs we have
		int count = 1;
		const char* name = exec.device().name();
		for (device_execute_interface& scan : execiter) {
			if (exec.device().type() == scan.device().type() && strcmp(name, scan.device().name()) == 0 && exec.device().clock() == scan.device().clock())
				if (exectags.insert(scan.device().tag()).second)
					count++;
		}

		std::string hz(std::to_string(clock));
		int d = (clock >= 1'000'000'000) ? 9 : (clock >= 1'000'000) ? 6 : (clock >= 1000) ? 3 : 0;
		if (d > 0) {
			size_t dpos = hz.length() - d;
			hz.insert(dpos, ".");
			size_t last = hz.find_last_not_of('0');
			hz = hz.substr(0, last + (last != dpos ? 1 : 0));
		}

		// if more than one, prepend a #x in front of the CPU name and display clock
		util::stream_format(cpu_str_buf,
			(count > 1) ? ((clock != 0) ? "%1$dx" "%2$s %3$s" "%4$s" : "%1$dx" "%2$s") : ((clock != 0) ? "%2$s %3$s" "%4$s" : "%2$s"), count, name, hz, (d == 9) ? _("GHz") : (d == 6) ? _("MHz\n") : (d == 3) ? _("kHz\n") : _("Hz\n"));

	}

	// loop over all sound chips
	sound_interface_enumerator snditer(machine.root_device());
	std::unordered_set<std::string> soundtags;
	bool found_sound = false;
	for (device_sound_interface& sound : snditer) {
		if (!sound.outputs() || !soundtags.insert(sound.device().tag()).second)
			continue;

		// append the Sound: string
		if (!found_sound)
			sound_buf << _("");
		found_sound = true;

		// count how many identical sound chips we have
		int count = 1;
		for (device_sound_interface& scan : snditer) {
			if (sound.device().type() == scan.device().type() && sound.device().clock() == scan.device().clock())
				if (soundtags.insert(scan.device().tag()).second)
					count++;
		}

		const u32 clock = sound.device().clock();
		std::string hz(std::to_string(clock));
		int d = (clock >= 1'000'000'000) ? 9 : (clock >= 1'000'000) ? 6 : (clock >= 1000) ? 3 : 0;
		if (d > 0) {
			size_t dpos = hz.length() - d;
			hz.insert(dpos, ".");
			size_t last = hz.find_last_not_of('0');
			hz = hz.substr(0, last + (last != dpos ? 1 : 0));
		}

		// if more than one, prepend a #x in front of the soundchip name and display clock
		util::stream_format(sound_buf,
			(count > 1)
			? ((clock != 0) ? "%1$dx" "%2$s %3$s" "%4$s" : "%1$dx" "%2$s")
			: ((clock != 0) ? "%2$s %3$s" "%4$s" : "%2$s"),
			count, sound.device().name(), hz,
			(d == 9) ? _("GHz\n") : (d == 6) ? _("MHz\n") : (d == 3) ? _("kHz\n") : _("Hz\n"));
	}

	// display screen information

	screen_device_enumerator scriter(machine.root_device());
	int scrcount = scriter.count();
	if (scrcount == 0)
		video_buf << _("None");
	else {
		for (screen_device& screen : scriter) {
			std::string detail;
			if (screen.screen_type() == SCREEN_TYPE_VECTOR)
				detail = _("Vector");
			else {
				const u32 rate = u32(screen.frame_period().as_hz() * 1'000'000 + 0.5);
				const bool valid = rate >= 1'000'000;
				std::string hz(valid ? std::to_string(rate) : "?");
				if (valid) {
					size_t dpos = hz.length() - 6;
					hz.insert(dpos, ".");
					size_t last = hz.find_last_not_of('0');
					hz = hz.substr(0, last + (last != dpos ? 1 : 0));
				}

				const rectangle& visarea = screen.visible_area();
				detail = string_format("%d " UTF8_MULTIPLY " %d (%s) %s" UTF8_NBSP "Hz\n",
					visarea.width(), visarea.height(),
					(screen.orientation() & ORIENTATION_SWAP_XY) ? "V" : "H",
					hz);
			}


			util::stream_format(video_buf,
				(scrcount > 1) ? _("%1$s: %2$s") : _("%2$s"),
				get_screen_desc(screen), detail);
		}
	}
	fullname_ = machine.config().gamedrv().type.fullname();
	name_ = machine.config().gamedrv().name;
	manufacturer_ = machine.config().gamedrv().manufacturer;
	year_ = machine.config().gamedrv().year;
	rom_ = machine.config().gamedrv().rom->name;
	flags_ = machine.config().gamedrv().flags;
	m_cpu_ = cpu_str_buf.str();
	driver_ = driver_buf.str();
	sound_ = sound_buf.str();
	video_ = video_buf.str();
}

GameDetails::GameDetails(const GameDetails& rhs) : machine_(rhs.machine_),
flags_() {
}

template <typename Writer>
void GameDetails::Serialize(Writer& writer) const {
	// This base class just write out name-value pairs, without wrapping within an object.
	writer.StartObject();
	writer.String("fullname");
	writer.String(fullname_.c_str(), static_cast<SizeType> (fullname_.length()));
	writer.String("name");
	writer.String(name_.c_str(), static_cast<SizeType> (name_.length()));
	writer.String("manufacturer");
	writer.String(manufacturer_.c_str(), static_cast<SizeType> (manufacturer_.length()));
	writer.String("year");
	writer.String(year_.c_str(), static_cast<SizeType> (year_.length()));
	writer.String("rom");
	writer.String(rom_.c_str(), static_cast<SizeType> (rom_.length()));
	writer.String("m_cpu");
	writer.String(m_cpu_.c_str(), static_cast<SizeType> (m_cpu_.length()));
	writer.String("sound");
	writer.String(sound_.c_str(), static_cast<SizeType> (sound_.length()));
	writer.String("video");
	writer.String(video_.c_str(), static_cast<SizeType> (video_.length()));
	writer.String("flags");
	writer.Uint(flags_);
	writer.EndObject();
}


//-------------------------------------------------
//  get_screen_desc - returns the description for
//  a given screen
//-------------------------------------------------

std::string GameDetails::get_screen_desc(screen_device& screen) const {
	if (screen_device_enumerator(machine_.root_device()).count() > 1)
		return string_format(_("Screen '%1$s'"), screen.tag());
	else
		return _("Screen");
}

GameDetails::~GameDetails() {
}
const vectrx2020_device::game_info_t vectrx2020_device::s_games[] = {
	{"armora", false, GAME_ARMORA, true},
	{"armorap", false, GAME_ARMORA, true},
	{"armorar", false, GAME_ARMORA, true},
	{"asteroid", false, GAME_NONE, true},
	{"asteroi1", false, GAME_NONE, true},
	{"astdelux", false, GAME_NONE, true},
	{"astdelu1", false, GAME_NONE, true},
	{"llander", false, GAME_NONE, true},
	{"llander1", false, GAME_NONE, true},
	{"demon", false, GAME_NONE, true},
	{"barrier", false, GAME_NONE, true},
	{"bzone", false, GAME_NONE, true},
	{"bzone2", false, GAME_NONE, true},
	{"bzonec", false, GAME_NONE, true},
	{"redbaron", false, GAME_NONE, true},
	{"omegrace", false, GAME_NONE, true},
	{"ripoff", false, GAME_NONE, true},
	{"solarq", false, GAME_NONE, true},
	{"speedfrk", false, GAME_NONE, true},
	{"starhawk", false, GAME_NONE, true},
	{"sundance", false, GAME_NONE, true},
	{"tailg", false, GAME_NONE, true},
	{"warrior", false, GAME_WARRIOR, true},
	{"wotw", false, GAME_NONE, true},
	{"spacewar", false, GAME_NONE, true},
	{"starcas", false, GAME_NONE, true},
	{"starcas1", false, GAME_NONE, true},
	{"starcasp", false, GAME_NONE, true},
	{"starcase", false, GAME_NONE, true},
	{"starwars", true, GAME_NONE, false},
	{"starwar1", true, GAME_NONE, false},
	{"esb", true, GAME_NONE, false},
	{ 0}
};

using namespace rapidjson;

vectrx2020_device::vectrx2020_device(const machine_config& mconfig, const char* tag, device_t* owner, uint32_t clock)
	: vector_interface(mconfig, VECTRX2020, tag, owner, clock),
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
	m_artwork(0),
	m_bw_game(false),
	m_vertical_display(0),
	m_st(),
	m_clipx_min(DVG_RES_MIN),
	m_clipx_max(DVG_RES_MAX),
	m_clipy_min(DVG_RES_MIN),
	m_clipy_max(DVG_RES_MAX)

{
}


void vectrx2020_device::device_start() {

	try {
		vector_interface::device_start();

		auto stdoutCb = std::make_shared<OutputCallback>(
			[](const char* msg) { printf("[DVG] %s\n", msg); }
		);
		auto stderrCb = std::make_shared<OutputCallback>(
			[](const char* msg) { fprintf(stderr, "[DVG ERR] %s\n", msg); }
		);

		m_vector_device = std::make_unique<WindowsDeviceHandler>(stdoutCb, stderrCb);

		if (!m_vector_device->IsInitialized()) {
			std::cerr << "Device handler failed to initialize." << std::endl;
			return;
		}

		const char* c_host = machine().config().options().vector_host();
		m_host = (c_host && c_host[0] != '\0') ? c_host : "vectrx2020";

		m_gamma_table = std::make_unique<float[]>(256);
		for (int i = 0; i < 256; i++) {
			m_gamma_table[i] = apply_brightness_contrast_gamma_fp(
				i,
				machine().options().brightness(),
				machine().options().contrast(),
				machine().options().gamma()
			);
		}


		m_centre_beam = machine().config().options().vector_centre_beam();

		if (!m_vector_device->Connect(m_host)) {
			std::cerr << "Device handler failed to Connect." << std::endl;
			return;
		}

		if (!m_vector_device->SendProtocol()) {
			std::cerr << "Failed to SendProtocol." << std::endl;
			return;
		}

		if (!get_dvg_info() || !get_dvg_settings() || !send_local_info()) {
			std::cerr << "Failed to initialize DVG settings." << std::endl;
			return;
		}

		// Load settings from MAME config directly into m_st.settings
		m_st.settings.dwell = machine().config().options().vector_dwell();
		m_st.settings.sync_mode = static_cast<m_sync_mode_t>(machine().config().options().vector_sync_mode());
		m_st.settings.draw_steps = machine().config().options().vector_draw_steps();
		m_st.settings.compressionLevel = static_cast<m_compression_level_t>(machine().config().options().vector_compression());
		m_st.settings.dac_period = machine().config().options().vector_dac_period();
		m_st.settings.scalex = machine().config().options().vector_scale_x() * 100;
		m_st.settings.scaley = machine().config().options().vector_scale_y() * 100;
		m_st.settings.x_offset = machine().config().options().vector_offset_x();
		m_st.settings.y_offset = machine().config().options().vector_offset_y();
		m_st.settings.orientation = static_cast<m_orientation_enum>(machine().config().options().vector_rotate());
		m_st.settings.rgb = machine().config().options().vector_rgb();


		if (!set_dvg_settings()) {
			std::cerr << "Failed to set_dvg_settings." << std::endl;
			return;
		}

	}
	catch (const std::exception& e) {
		std::cerr << "An exception occurred: " << e.what() << std::endl;
	}
	catch (...) {
		std::cerr << "An unknown exception occurred." << std::endl;
	}
}

int vectrx2020_device::get_dwell() const { return m_st.settings.dwell; }
m_sync_mode_t vectrx2020_device::get_sync_mode() const { return m_st.settings.sync_mode; }
int vectrx2020_device::get_draw_steps() const { return m_st.settings.draw_steps; }
m_compression_level_t vectrx2020_device::get_compression() const { return m_st.settings.compressionLevel; }
int vectrx2020_device::get_dac_period() const { return m_st.settings.dac_period; }
int vectrx2020_device::get_scale_x() const { return m_st.settings.scalex; }
int vectrx2020_device::get_scale_y() const { return m_st.settings.scaley; }
int vectrx2020_device::get_x_offset() const { return m_st.settings.x_offset; }
int vectrx2020_device::get_y_offset() const { return m_st.settings.y_offset; }
m_orientation_enum vectrx2020_device::get_orientation() const { return m_st.settings.orientation; }
bool vectrx2020_device::get_rgb() const { return m_st.settings.rgb; }

void vectrx2020_device::set_vector_host(const char* host)
{

	machine().config().options().set_value(OPTION_VECTOR_HOST, host, OPTION_PRIORITY_CMDLINE);
}

void vectrx2020_device::set_dac_period(int period)
{
	if (m_st.settings.dac_period == period)
		return;

	m_st.settings.dac_period = period;
	set_dvg_settings();

	machine().config().options().set_value(
		"vectrx2020_dac_period",
		period,
		OPTION_PRIORITY_CMDLINE
	);
}

void vectrx2020_device::set_dwell(int dwell)
{
	if (m_st.settings.dwell == dwell)
		return;

	m_st.settings.dwell = dwell;
	set_dvg_settings();

	machine().config().options().set_value(
		"vectrx2020_dwell",
		dwell,
		OPTION_PRIORITY_CMDLINE
	);
}

void vectrx2020_device::set_sync_mode(m_sync_mode_t mode)
{
	if (m_st.settings.sync_mode == mode)
		return;

	m_st.settings.sync_mode = mode;
	set_dvg_settings();

	machine().config().options().set_value(
		"vectrx2020_sync_mode",
		int(mode),
		OPTION_PRIORITY_CMDLINE
	);
}

void vectrx2020_device::set_draw_steps(int steps)
{
	if (m_st.settings.draw_steps == steps)
		return;

	m_st.settings.draw_steps = steps;
	set_dvg_settings();

	machine().config().options().set_value(
		"vectrx2020_draw_steps",
		steps,
		OPTION_PRIORITY_CMDLINE
	);
}

void vectrx2020_device::set_compression(m_compression_level_t level)
{
	if (m_st.settings.compressionLevel == level)
		return;

	m_st.settings.compressionLevel = level;
	set_dvg_settings();

	machine().config().options().set_value(
		"vectrx2020_compression",
		int(level),
		OPTION_PRIORITY_CMDLINE
	);
}

void vectrx2020_device::set_scale_x(int percent)
{
	if (m_st.settings.scalex == percent)
		return;

	m_st.settings.scalex = percent;
	set_dvg_settings();

	machine().config().options().set_value(
		"vectrx2020_scale_x",
		percent / 100.0f,
		OPTION_PRIORITY_CMDLINE
	);
}

void vectrx2020_device::set_scale_y(int percent)
{
	if (m_st.settings.scaley == percent)
		return;

	m_st.settings.scaley = percent;
	set_dvg_settings();

	machine().config().options().set_value(
		"vectrx2020_scale_y",
		percent / 100.0f,
		OPTION_PRIORITY_CMDLINE
	);
}

void vectrx2020_device::set_offset_x(int offset)
{
	if (m_st.settings.x_offset == offset)
		return;

	m_st.settings.x_offset = offset;
	set_dvg_settings();

	machine().config().options().set_value(
		"vectrx2020_offset_x",
		offset,
		OPTION_PRIORITY_CMDLINE
	);
}

void vectrx2020_device::set_offset_y(int offset)
{
	if (m_st.settings.y_offset == offset)
		return;

	m_st.settings.y_offset = offset;
	set_dvg_settings();

	machine().config().options().set_value(
		"vectrx2020_offset_y",
		offset,
		OPTION_PRIORITY_CMDLINE
	);
}

void vectrx2020_device::set_orientation(m_orientation_enum orient)
{
	if (m_st.settings.orientation == orient)
		return;

	m_st.settings.orientation = orient;
	set_dvg_settings();

	machine().config().options().set_value(
		"vectrx2020_rotate",
		int(orient),
		OPTION_PRIORITY_CMDLINE
	);
}

void vectrx2020_device::set_rgb(bool enable)
{
	if (m_st.settings.rgb == enable)
		return;

	m_st.settings.rgb = enable;
	set_dvg_settings();

	machine().config().options().set_value(
		"vectrx2020_rgb",
		enable,
		OPTION_PRIORITY_CMDLINE
	);
}

bool vectrx2020_device::get_dvg_settings() {
	return m_vector_device->ReceiveDvgSettings(m_st);
}

bool vectrx2020_device::set_dvg_settings() {
	return m_vector_device->SendDvgSettings(m_st);
}

void vectrx2020_device::device_reset() {
}

void vectrx2020_device::device_stop() {
	try {
		// Send a command to the device to signal that we're about to close the connection
		m_vector_device->SendExit();

		// Perform any additional cleanup if necessary
		// ...

		// Log that the device was stopped successfully
		osd_printf_info("Device connection closed gracefully.\n");
	}
	catch (const std::exception& e) {
		// Log any exceptions that occur during shutdown
		osd_printf_error("An exception occurred while stopping the device: %s\n", e.what());
	}
	catch (...) {
		// Catch any other exceptions
		osd_printf_error("An unknown exception occurred while stopping the device.\n");
	}
}
uint32_t vectrx2020_device::compute_code(int32_t x, int32_t y) {
	// initialized as being inside 
	uint32_t code = 0;

	if (x < m_clipx_min) // to the left of rectangle 
		code |= LEFT;
	else if (x > m_clipx_max) // to the right of rectangle 
		code |= RIGHT;
	if (y < m_clipy_min) // below the rectangle 
		code |= BOTTOM;
	else if (y > m_clipy_max) // above the rectangle 
		code |= TOP;

	return code;
}

//
// Cohen-Sutherland line-clipping algorithm.  
//

uint32_t vectrx2020_device::line_clip(int32_t* pX1, int32_t* pY1, int32_t* pX2, int32_t* pY2) {
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

	while (1) {
		if ((code1 == 0) && (code2 == 0)) {
			// If both endpoints lie within rectangle 
			accept = 1;
			break;
		}
		else if (code1 & code2) {
			// If both endpoints are outside rectangle, 
			// in same region 
			break;
		}
		else {
			// Some segment of line lies within the 
			// rectangle 
			// At least one endpoint is outside the 
			// rectangle, pick it. 
			if (code1 != 0) {
				code_out = code1;
			}
			else {
				code_out = code2;
			}

			// Find intersection point; 
			// using formulas y = y1 + slope * (x - x1), 
			// x = x1 + (1 / slope) * (y - y1) 
			if (code_out & TOP) {
				// point is above the clip rectangle 
				x = x1 + (x2 - x1) * (m_clipy_max - y1) / (y2 - y1);
				y = m_clipy_max;
			}
			else if (code_out & BOTTOM) {
				// point is below the rectangle 
				x = x1 + (x2 - x1) * (m_clipy_min - y1) / (y2 - y1);
				y = m_clipy_min;
			}
			else if (code_out & RIGHT) {
				// point is to the right of rectangle 
				y = y1 + (y2 - y1) * (m_clipx_max - x1) / (x2 - x1);
				x = m_clipx_max;
			}
			else if (code_out & LEFT) {
				// point is to the left of rectangle 
				y = y1 + (y2 - y1) * (m_clipx_min - x1) / (x2 - x1);
				x = m_clipx_min;
			}

			// Now intersection point x, y is found 
			// We replace point outside rectangle 
			// by intersection point 
			if (code_out == code1) {
				x1 = x;
				y1 = y;
				code1 = compute_code(x1, y1);
			}
			else {
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


static bool have_first_point = false;
void vectrx2020_device::cmd_reset(uint32_t initial) {
	m_points.clear();
	have_first_point = false;
}

void vectrx2020_device::normalize_and_push(int x, int y, ::rgb_t color) {
	dvg_pnt point;

	x = std::clamp(x, 0, DVG_RES_MAX);
	y = std::clamp(y, 0, DVG_RES_MAX);

	point.pnt.x = ((float)x) / 4096.0f;
	point.pnt.y = ((float)y) / 4096.0f;

	// Check if this is the first point being added to the vector
	if (m_points.empty()) {
		// If it's the first point, set the color to black
		point.color.r = 0.0f;
		point.color.g = 0.0f;
		point.color.b = 0.0f;
	}
	else {
		// Otherwise, use the provided color
		point.color.r = ((float)color.r()) / 255.0f;
		point.color.g = ((float)color.g()) / 255.0f;
		point.color.b = ((float)color.b()) / 255.0f;
	}

	m_points.push_back(point);
}

void vectrx2020_device::transform_and_scale_coords(int* px, int* py) {
	float x, y;

	x = (*px >> 16) + (*px & 0xffff) / 65536.0;
	x *= m_xscale;
	y = (*py >> 16) + (*py & 0xffff) / 65536.0;
	y *= m_yscale;
	*px = x;
	*py = y;
}

bool vectrx2020_device::get_dvg_info() {


	// Get the version from the emulator info
	uint32_t major, minor;
	sscanf(emulator_info::get_build_version(), "%u.%u", &major, &minor);
	uint32_t version = (((minor / 1000) % 10) << 12) | (((minor / 100) % 10) << 8) | (((minor / 10) % 10) << 4) | (minor % 10);
	version |= (major << 16);

	// Convert the version number to a string
	std::ostringstream versionStream;
	versionStream << std::hex << version; // Convert to hexadecimal string
	std::string versionString = versionStream.str();

	// Prepare an empty string to receive the JSON output
	std::string jsonOutput;

	// Call GetDvgInfo with the version string and the output string
	bool result = m_vector_device->GetDvgInfo(versionString, jsonOutput);

	if (!result) {
		std::cerr << "Failed to receive packet data." << std::endl;
		return false;
	}

	// Parse the JSON output
	Document d;
	d.Parse(jsonOutput.c_str());

	if (!d.HasParseError()) {
		m_flip_y = false;
		m_vertical_display = d["vertical"].GetBool();


		if (machine().config().gamedrv().flags & machine_flags::SWAP_XY) {
			std::cout << "SWAP_XY " << std::endl;
			m_swap_xy = true;
		}
		if (machine().config().gamedrv().flags & machine_flags::FLIP_Y) {
			std::cout << "FLIP_Y " << std::endl;
			m_flip_y = true;
		}
		if (machine().config().gamedrv().flags & machine_flags::FLIP_X) {
			std::cout << "FLIP_X " << std::endl;
			m_flip_x = true;
		}

		m_bw_game = false;
		m_artwork = GAME_NONE;
		m_exclude_blank_vectors = false;
		if (!strcmp(emulator_info::get_appname_lower(), "mess")) {
			m_bw_game = 1;
		}
		else {
			for (int i = 0; s_games[i].name; i++) {
				if (!strcmp(machine().system().name, s_games[i].name)) {
					m_artwork = s_games[i].artwork;
					m_exclude_blank_vectors = s_games[i].exclude_blank_vectors;
					m_bw_game = s_games[i].bw_game;
					break;
				}
			}
		}
	}
	else
	{
		return false;
	}
	//todo: check if need for RGB currently mhavoc needs this to be true
	m_exclude_blank_vectors = true;
	return true;

}

void vectrx2020_device::transform_final(int* px, int* py) {
	int x, y, tmp;
	x = *px;
	y = *py;

	if (m_swap_xy) {
		tmp = x;
		x = y;
		y = tmp;
	}
	if (m_flip_x) {
		x = DVG_RES_MAX - x;
	}
	if (m_flip_y) {
		y = DVG_RES_MAX - y;
	}
	x = std::clamp(x, 0, DVG_RES_MAX);
	y = std::clamp(y, 0, DVG_RES_MAX);

	if (m_vertical_display) {
		if (!m_swap_xy) {

			y = 512 + (0.75 * y);
		}
	}
	else
	{
		if (m_swap_xy) {
			// Vertical on horizontal display
			x = 512 + (0.75 * x);
		}
	}

	*px = x;
	*py = y;
}





void vectrx2020_device::add_point(int x, int y, ::rgb_t color, int intensity) {

	static int last_x = 0, last_y = 0;
	static ::rgb_t last_color;
	bool add = true;

	if (machine().config().options().debug())
	{
		if (!have_first_point)
		{
			osd_printf_info("----NEW FRAME ---\n");
		}
		osd_printf_info("x:%d y:%d r:%u g:%u b:%u i:%d\n", x, y, static_cast<unsigned>(color.r()), static_cast<unsigned>(color.g()), static_cast<unsigned>(color.b()), intensity);
	}
	// Clamp intensity and handle color

	if ((intensity == 0 || color == ::rgb_t::black()) && !m_centre_beam) {
		color = ::rgb_t::black();
	}
	else
	{
		float cscale = m_gamma_table[intensity];
		// Ensure cscale is within a range that won't cause overflow when multiplied by color components
		cscale = std::clamp(cscale, 0.0f, 1.0f);

		if (m_st.settings.rgb) {


			// Calculate new color values with proper casting and clamping
			uint8_t new_r = static_cast<uint8_t>(std::clamp(static_cast<int>(cscale * color.r()), 0, 255));
			uint8_t new_g = static_cast<uint8_t>(std::clamp(static_cast<int>(cscale * color.g()), 0, 255));
			uint8_t new_b = static_cast<uint8_t>(std::clamp(static_cast<int>(cscale * color.b()), 0, 255));


			//new_r = 255;
			//new_g = 255;
			//new_b = 255;
			// Set the new color values
			color.set_r(new_r);
			color.set_g(new_g);
			color.set_b(new_b);
		}
		else
		{
			color.set_r(cscale * 255);
			color.set_g(cscale * 255);
			color.set_b(cscale * 255);
		}
	}


	bool blank = (color == ::rgb_t::black());

	if ((last_x == x) && (last_y == y) && blank)
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

	// Check if this is the start of a new line or the end of the current line
	if (!have_first_point) {
		//  the point and its color will be last values for next point

		have_first_point = true;
	}
	//else if(add && (m_st.settings.rgb || !blank))
	else if (add)
	{
		// We have a line segment; clip and add it
		int clipped_x1 = last_x;
		int clipped_y1 = last_y;
		int clipped_x2 = x;
		int clipped_y2 = y;

		// Transform and scale coordinates
		transform_and_scale_coords(&clipped_x1, &clipped_y1);
		transform_and_scale_coords(&clipped_x2, &clipped_y2);

		// Clip the line using the Cohen-Sutherland algorithm
		uint32_t accept = line_clip(&clipped_x1, &clipped_y1, &clipped_x2, &clipped_y2);

		// If the line is accepted after clipping, add it to the vector
		if (accept) {
			// Normalize and push the first point

			normalize_and_push(clipped_x1, clipped_y1, last_color);
			// Normalize and push the second point
			normalize_and_push(clipped_x2, clipped_y2, color);
		}

		// The current point becomes the last point for the next line segment

	}
	last_x = x;
	last_y = y;
	last_color = color;
}

bool vectrx2020_device::send_vectors() {
	bool result = m_vector_device->SendPoints(m_points.data(), m_points.size());
	cmd_reset(0);
	return result;
}


bool vectrx2020_device::send_local_info() {
	rapidjson::StringBuffer sb;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);
	GameDetails(machine()).Serialize(writer);
	const char* json = sb.GetString();
	uint16_t json_len = static_cast<uint16_t> (sb.GetSize());
	std::cout << "Game info: " << json << std::endl;
	return  m_vector_device->SendClientInfo((char*)json, json_len);
}



uint32_t vectrx2020_device::screen_update(screen_device& screen, bitmap_rgb32& bitmap, const rectangle& cliprect) {

	::rgb_t color = ::rgb_t(108, 108, 108);
	::rgb_t black = ::rgb_t::black();



	m_xmin = screen.visible_area().min_x;
	m_xmax = screen.visible_area().max_x;
	m_ymin = screen.visible_area().min_y;
	m_ymax = screen.visible_area().max_y;

	if (m_xmax == 0) {
		m_xmax = 1;
	}
	if (m_ymax == 0) {
		m_ymax = 1;
	}

	m_xscale = (DVG_RES_MAX + 1.0) / (m_xmax - m_xmin);
	m_yscale = (DVG_RES_MAX + 1.0) / (m_ymax - m_ymin);

	if (m_points.size()) {
		switch (m_artwork) {
		case GAME_ARMORA:
			// Upper Right Quadrant
			// Outer structure
			normalize_and_push(3446, 2048, black);
			normalize_and_push(3958, 2048, color);
			normalize_and_push(3958, 2224, color);
			normalize_and_push(3958, 3059, color);
			normalize_and_push(3323, 3059, color);
			normalize_and_push(3323, 3225, color);
			normalize_and_push(3194, 3225, color);
			normalize_and_push(3194, 3393, color);
			normalize_and_push(3067, 3393, color);
			normalize_and_push(3067, 3901, color);
			normalize_and_push(2304, 3901, color);
			normalize_and_push(2304, 3225, color);
			normalize_and_push(2048, 3225, color);
			// Center structure
			normalize_and_push(2048, 2373, black);
			normalize_and_push(2562, 2373, color);
			normalize_and_push(2562, 2738, color);
			normalize_and_push(2430, 2738, color);
			normalize_and_push(2430, 2893, color);
			normalize_and_push(2306, 2893, color);
			normalize_and_push(2306, 3065, color);
			normalize_and_push(2048, 3065, color);
			// Big structure
			normalize_and_push(2938, 2209, black);
			normalize_and_push(3198, 2209, color);
			normalize_and_push(3198, 2383, color);
			normalize_and_push(3706, 2383, color);
			normalize_and_push(3706, 2738, color);
			normalize_and_push(2938, 2738, color);
			normalize_and_push(2938, 2209, color);
			// Small structure
			normalize_and_push(2551, 3055, black);
			normalize_and_push(2551, 3590, color);
			normalize_and_push(2816, 3590, color);
			normalize_and_push(2422, 3590, color);
			normalize_and_push(2422, 3231, color);
			normalize_and_push(2555, 3231, color);
			normalize_and_push(2555, 3055, color);
			// Upper Left Quadrant
			// Outer structure
			normalize_and_push(649, 2048, black);
			normalize_and_push(649, 2224, color);
			normalize_and_push(137, 2224, color);
			normalize_and_push(137, 3059, color);
			normalize_and_push(772, 3059, color);
			normalize_and_push(772, 3225, color);
			normalize_and_push(901, 3225, color);
			normalize_and_push(901, 3393, color);
			normalize_and_push(1028, 3393, color);
			normalize_and_push(1028, 3901, color);
			normalize_and_push(1792, 3901, color);
			normalize_and_push(1792, 3225, color);
			normalize_and_push(2048, 3225, color);
			// Center structure
			normalize_and_push(2048, 2373, black);
			normalize_and_push(2048, 2738, color);
			normalize_and_push(1533, 2738, color);
			normalize_and_push(1665, 2738, color);
			normalize_and_push(1665, 2893, color);
			normalize_and_push(1789, 2893, color);
			normalize_and_push(1789, 3065, color);
			normalize_and_push(2048, 3065, color);
			// Big structure
			normalize_and_push(1157, 2209, black);
			normalize_and_push(1157, 2383, color);
			normalize_and_push(897, 2383, color);
			normalize_and_push(389, 2383, color);
			normalize_and_push(389, 2738, color);
			normalize_and_push(1157, 2738, color);
			normalize_and_push(1157, 2209, color);
			// Small structure
			normalize_and_push(1544, 3055, black);
			normalize_and_push(1544, 3590, color);
			normalize_and_push(1280, 3590, color);
			normalize_and_push(1673, 3590, color);
			normalize_and_push(1673, 3231, color);
			normalize_and_push(1540, 3231, color);
			normalize_and_push(1540, 3055, color);
			// Lower Right Quadrant
			// Outer structure
			normalize_and_push(3446, 2048, black);
			normalize_and_push(3446, 1871, color);
			normalize_and_push(3958, 1871, color);
			normalize_and_push(3958, 1036, color);
			normalize_and_push(3323, 1036, color);
			normalize_and_push(3323, 870, color);
			normalize_and_push(3194, 870, color);
			normalize_and_push(3194, 702, color);
			normalize_and_push(3067, 702, color);
			normalize_and_push(3067, 194, color);
			normalize_and_push(2304, 194, color);
			normalize_and_push(2304, 870, color);
			normalize_and_push(2048, 870, color);
			// Center structure
			normalize_and_push(2048, 1722, black);
			normalize_and_push(2048, 1357, color);
			normalize_and_push(2562, 1357, color);
			normalize_and_push(2430, 1357, color);
			normalize_and_push(2430, 1202, color);
			normalize_and_push(2306, 1202, color);
			normalize_and_push(2306, 1030, color);
			normalize_and_push(2048, 1030, color);
			// Big structure
			normalize_and_push(2938, 1886, black);
			normalize_and_push(2938, 1712, color);
			normalize_and_push(3198, 1712, color);
			normalize_and_push(3706, 1712, color);
			normalize_and_push(3706, 1357, color);
			normalize_and_push(2938, 1357, color);
			normalize_and_push(2938, 1886, color);
			// Small structure
			normalize_and_push(2551, 1040, black);
			normalize_and_push(2551, 505, color);
			normalize_and_push(2816, 505, color);
			normalize_and_push(2422, 505, color);
			normalize_and_push(2422, 864, color);
			normalize_and_push(2555, 864, color);
			normalize_and_push(2555, 1040, color);
			// Lower Left Quadrant
			// Outer structure
			normalize_and_push(649, 2048, black);
			normalize_and_push(649, 1871, color);
			normalize_and_push(137, 1871, color);
			normalize_and_push(137, 1036, color);
			normalize_and_push(772, 1036, color);
			normalize_and_push(772, 870, color);
			normalize_and_push(901, 870, color);
			normalize_and_push(901, 702, color);
			normalize_and_push(1028, 702, color);
			normalize_and_push(1028, 194, color);
			normalize_and_push(1792, 194, color);
			normalize_and_push(1792, 870, color);
			normalize_and_push(2048, 870, color);
			// Center structure
			normalize_and_push(2048, 1722, black);
			normalize_and_push(2048, 1357, color);
			normalize_and_push(1533, 1357, color);
			normalize_and_push(1665, 1357, color);
			normalize_and_push(1665, 1202, color);
			normalize_and_push(1789, 1202, color);
			normalize_and_push(1789, 1030, color);
			normalize_and_push(2048, 1030, color);
			// Big structure
			normalize_and_push(1157, 1886, black);
			normalize_and_push(1157, 1712, color);
			normalize_and_push(897, 1712, color);
			normalize_and_push(389, 1712, color);
			normalize_and_push(389, 1357, color);
			normalize_and_push(1157, 1357, color);
			normalize_and_push(1157, 1886, color);
			// Small structure
			normalize_and_push(1544, 1040, black);
			normalize_and_push(1544, 505, color);
			normalize_and_push(1280, 505, color);
			normalize_and_push(1673, 505, color);
			normalize_and_push(1673, 864, color);
			normalize_and_push(1540, 864, color);
			normalize_and_push(1540, 1040, color);
			break;

		case GAME_WARRIOR:
			normalize_and_push(1187, 2232, black);
			normalize_and_push(1863, 2232, color);
			normalize_and_push(1187, 1372, black);
			normalize_and_push(1863, 1372, color);
			normalize_and_push(1187, 2232, black);
			normalize_and_push(1187, 1372, color);
			normalize_and_push(1863, 2232, black);
			normalize_and_push(1863, 1372, color);
			normalize_and_push(2273, 2498, black);
			normalize_and_push(2949, 2498, color);
			normalize_and_push(2273, 1658, black);
			normalize_and_push(2949, 1658, color);
			normalize_and_push(2273, 2498, black);
			normalize_and_push(2273, 1658, color);
			normalize_and_push(2949, 2498, black);
			normalize_and_push(2949, 1658, color);
			break;
		}


		send_vectors();
	}

	return 0;
}
