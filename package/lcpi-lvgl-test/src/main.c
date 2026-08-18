// SPDX-License-Identifier: MIT
/*
 * LCPI-PC-T113 automatic EP4303B/GT911 bring-up UI.
 *
 * The app starts by itself through the fbdev compatibility device. This safe
 * baseline deliberately avoids an LVGL atomic DRM page flip until the panel
 * path has been proven on real hardware. It discovers the Goodix
 * multitouch event device explicitly because mainline GT911 exposes
 * ABS_MT_POSITION_X/Y instead of the ABS_X/Y pair used by LVGL auto-discovery.
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/input.h>
#include <linux/kd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "lvgl.h"
#include "demos/benchmark/lv_demo_benchmark.h"
#include "src/drivers/display/drm/lv_linux_drm.h"
#include "src/drivers/display/fb/lv_linux_fbdev.h"
#include "src/drivers/evdev/lv_evdev.h"

#define EXPECTED_WIDTH  480
#define EXPECTED_HEIGHT 272
#define TARGET_COUNT    5

enum menu_target {
	MENU_HOME = 0,
	MENU_DISPLAY,
	MENU_TOUCH,
	MENU_BENCHMARK,
	MENU_RESET_TOUCH,
	MENU_STATUS,
};

static volatile sig_atomic_t stop_requested;
static int console_fd = -1;

static lv_display_t *display;
static lv_indev_t *touch;
static lv_obj_t *tileview;
static lv_obj_t *tiles[3];
static lv_obj_t *page_indicator;
static lv_obj_t *display_status_home;
static lv_obj_t *display_status_page;
static lv_obj_t *touch_status_home;
static lv_obj_t *touch_status_page;
static lv_obj_t *touch_result;
static lv_obj_t *coordinate_label;
static lv_obj_t *cursor;
static lv_obj_t *targets[TARGET_COUNT];
static lv_obj_t *benchmark_button_label;
static lv_obj_t *moving_block;
static lv_obj_t *status_sheet;
static lv_timer_t *touch_scan_timer;
static lv_timer_t *touch_feedback_timer;
static lv_timer_t *refresh_marker_timer;
static lv_timer_t *benchmark_launch_timer;

static int32_t screen_width;
static int32_t screen_height;
static unsigned int targets_hit;
static int32_t moving_x;
static int32_t moving_step = 2;

static void signal_handler(int signum)
{
	(void)signum;
	stop_requested = 1;
}

static void hide_framebuffer_console(void)
{
	console_fd = open("/dev/tty0", O_RDWR | O_NOCTTY);
	if (console_fd >= 0)
		(void)ioctl(console_fd, KDSETMODE, KD_GRAPHICS);
}

static void restore_framebuffer_console(void)
{
	if (console_fd >= 0) {
		(void)ioctl(console_fd, KDSETMODE, KD_TEXT);
		(void)close(console_fd);
		console_fd = -1;
	}
}

static void set_label_color(lv_obj_t *label, uint32_t rgb)
{
	if (label != NULL)
		lv_obj_set_style_text_color(label, lv_color_hex(rgb), 0);
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *text,
				    const lv_font_t *font, uint32_t rgb,
				    int32_t x, int32_t y)
{
	lv_obj_t *label = lv_label_create(parent);

	lv_label_set_text(label, text);
	lv_obj_set_pos(label, x, y);
	lv_obj_set_style_text_font(label, font, 0);
	set_label_color(label, rgb);
	return label;
}

static lv_obj_t *make_box(lv_obj_t *parent, int32_t x, int32_t y,
			  int32_t width, int32_t height, uint32_t color,
			  int32_t radius)
{
	lv_obj_t *box = lv_obj_create(parent);

	lv_obj_remove_flag(box, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_pos(box, x, y);
	lv_obj_set_size(box, width, height);
	lv_obj_set_style_border_width(box, 0, 0);
	lv_obj_set_style_pad_all(box, 0, 0);
	lv_obj_set_style_radius(box, radius, 0);
	lv_obj_set_style_bg_color(box, lv_color_hex(color), 0);
	lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);

	return box;
}

static void update_display_status(void)
{
	char text[96];
	const char *mode = "FBDEV SAFE";
	uint32_t color = 0x7cf7c4;

	if (screen_width != EXPECTED_WIDTH || screen_height != EXPECTED_HEIGHT) {
		(void)snprintf(text, sizeof(text), "DISPLAY ERROR: %ldx%ld (need 480x272)",
			       (long)screen_width, (long)screen_height);
		color = 0xff6b6b;
	} else {
		(void)snprintf(text, sizeof(text), "DISPLAY: %s  %ldx%ld", mode,
			       (long)screen_width, (long)screen_height);
	}
	if (display_status_home != NULL)
		lv_label_set_text(display_status_home, text);
	if (display_status_page != NULL)
		lv_label_set_text(display_status_page, text);
	set_label_color(display_status_home, color);
	set_label_color(display_status_page, color);
}

static void set_touch_status(const char *text, uint32_t rgb)
{
	if (touch_status_home != NULL)
		lv_label_set_text(touch_status_home, text);
	if (touch_status_page != NULL)
		lv_label_set_text(touch_status_page, text);
	set_label_color(touch_status_home, rgb);
	set_label_color(touch_status_page, rgb);
}

static bool input_has_mt_axes(int fd, struct input_absinfo *x_axis,
			      struct input_absinfo *y_axis)
{
	unsigned long abs_bits[(ABS_MAX / (sizeof(unsigned long) * CHAR_BIT)) + 1] = {0};
	const unsigned long bits_per_long = sizeof(unsigned long) * CHAR_BIT;
	bool has_x;
	bool has_y;

	if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs_bits)), abs_bits) < 0)
		return false;

	has_x = (abs_bits[ABS_MT_POSITION_X / bits_per_long] >>
		 (ABS_MT_POSITION_X % bits_per_long)) & 1UL;
	has_y = (abs_bits[ABS_MT_POSITION_Y / bits_per_long] >>
		 (ABS_MT_POSITION_Y % bits_per_long)) & 1UL;

	if (!has_x || !has_y)
		return false;

	if (ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), x_axis) < 0)
		return false;
	if (ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), y_axis) < 0)
		return false;

	return x_axis->maximum > x_axis->minimum &&
	       y_axis->maximum > y_axis->minimum;
}

static void try_attach_goodix(void)
{
	DIR *input_dir;
	struct dirent *entry;

	if (touch != NULL)
		return;

	input_dir = opendir("/dev/input");
	if (input_dir == NULL) {
		set_touch_status("GT911: waiting for /dev/input", 0xff6b6b);
		return;
	}

	while ((entry = readdir(input_dir)) != NULL) {
		char dev_path[PATH_MAX];
		char name[256] = {0};
		struct input_absinfo x_axis;
		struct input_absinfo y_axis;
		int fd;
		int name_len;

		if (strncmp(entry->d_name, "event", 5) != 0)
			continue;

		if (snprintf(dev_path, sizeof(dev_path), "/dev/input/%s",
			     entry->d_name) >= (int)sizeof(dev_path))
			continue;

		fd = open(dev_path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
		if (fd < 0)
			continue;

		name_len = ioctl(fd, EVIOCGNAME(sizeof(name)), name);
		if (name_len < 0 ||
		    (strcmp(name, "Goodix Capacitive TouchScreen") != 0 &&
		     strstr(name, "Goodix") == NULL) ||
		    !input_has_mt_axes(fd, &x_axis, &y_axis)) {
			(void)close(fd);
			continue;
		}

		/* lv_evdev_create_fd takes ownership only when it succeeds. */
		touch = lv_evdev_create_fd(LV_INDEV_TYPE_POINTER, fd);
		if (touch == NULL) {
			(void)close(fd);
			continue;
		}

		lv_evdev_set_calibration(touch, x_axis.minimum, y_axis.minimum,
					 x_axis.maximum, y_axis.maximum);
		lv_indev_set_display(touch, display);
		set_touch_status("GT911: READY  -  swipe left/right", 0x7cf7c4);
		closedir(input_dir);
		return;
	}

	closedir(input_dir);
	set_touch_status("GT911: WAITING", 0xff6b6b);
}

static void update_touch_feedback(lv_timer_t *timer)
{
	lv_point_t point;
	char text[64];

	(void)timer;
	if (touch == NULL || coordinate_label == NULL || cursor == NULL ||
	    tileview == NULL ||
	    lv_tileview_get_tile_active(tileview) != tiles[MENU_TOUCH])
		return;

	lv_indev_get_point(touch, &point);
	(void)snprintf(text, sizeof(text), "Touch: %ld, %ld",
		       (long)point.x, (long)point.y);
	lv_label_set_text(coordinate_label, text);

	if (lv_indev_get_state(touch) == LV_INDEV_STATE_PRESSED) {
		lv_obj_set_pos(cursor, point.x - 5, point.y - 5);
		lv_obj_clear_flag(cursor, LV_OBJ_FLAG_HIDDEN);
	} else {
		lv_obj_add_flag(cursor, LV_OBJ_FLAG_HIDDEN);
	}
}

static void scan_touch_timer(lv_timer_t *timer)
{
	(void)timer;
	try_attach_goodix();
}

static void move_refresh_marker(lv_timer_t *timer)
{
	(void)timer;
	if (moving_block == NULL || tileview == NULL ||
	    lv_tileview_get_tile_active(tileview) != tiles[MENU_DISPLAY])
		return;

	moving_x += moving_step;
	if (moving_x <= 0 || moving_x + 22 >= screen_width) {
		moving_step = -moving_step;
		moving_x += moving_step;
	}
	lv_obj_set_x(moving_block, moving_x);
}

static void update_touch_result(void)
{
	char text[64];

	if (touch_result == NULL)
		return;

	if (targets_hit == ((1U << TARGET_COUNT) - 1U)) {
		lv_label_set_text(touch_result, "TOUCH PASS");
		set_label_color(touch_result, 0x7cf7c4);
		return;
	}

	(void)snprintf(text, sizeof(text), "Touch targets: %u/%u",
		       (unsigned int)__builtin_popcount(targets_hit), TARGET_COUNT);
	lv_label_set_text(touch_result, text);
	set_label_color(touch_result, 0xffffff);
}

static void reset_touch_targets(void)
{
	unsigned int index;

	targets_hit = 0;
	for (index = 0; index < TARGET_COUNT; ++index) {
		if (targets[index] == NULL)
			continue;
		lv_obj_set_style_bg_color(targets[index], lv_color_hex(0xdc2626), 0);
		lv_obj_set_style_border_color(targets[index], lv_color_hex(0xffffff), 0);
	}
	update_touch_result();
}

static void target_clicked(lv_event_t *event)
{
	unsigned int index = (unsigned int)(uintptr_t)lv_event_get_user_data(event);

	if (index >= TARGET_COUNT || targets[index] == NULL)
		return;

	targets_hit |= 1U << index;
	lv_obj_set_style_bg_color(targets[index], lv_color_hex(0x36d399), 0);
	lv_obj_set_style_border_color(targets[index], lv_color_hex(0xffffff), 0);
	update_touch_result();
}

static void start_benchmark(lv_timer_t *timer)
{
	if (timer != NULL)
		lv_timer_delete(timer);
	benchmark_launch_timer = NULL;

	if (touch_scan_timer != NULL) {
		lv_timer_delete(touch_scan_timer);
		touch_scan_timer = NULL;
	}
	if (touch_feedback_timer != NULL) {
		lv_timer_delete(touch_feedback_timer);
		touch_feedback_timer = NULL;
	}
	if (refresh_marker_timer != NULL) {
		lv_timer_delete(refresh_marker_timer);
		refresh_marker_timer = NULL;
	}
	if (status_sheet != NULL) {
		lv_obj_delete(status_sheet);
		status_sheet = NULL;
	}

	/* The upstream benchmark clears the active screen and owns the UI. */
	lv_sysmon_hide_performance(display);
	lv_demo_benchmark();
}

static void schedule_benchmark(lv_event_t *event)
{
	(void)event;
	if (benchmark_launch_timer != NULL)
		return;

	if (benchmark_button_label != NULL)
		lv_label_set_text(benchmark_button_label, "Benchmark starting...");
	benchmark_launch_timer = lv_timer_create(start_benchmark, 250, NULL);
}

static void close_status_sheet(lv_event_t *event)
{
	(void)event;
	if (status_sheet != NULL) {
		lv_obj_delete(status_sheet);
		status_sheet = NULL;
	}
}

static void show_status_sheet(void)
{
	char summary[192];
	lv_obj_t *card;
	lv_obj_t *close_button;
	lv_obj_t *close_label;
	lv_obj_t *body;

	if (status_sheet != NULL)
		return;

	status_sheet = lv_obj_create(lv_screen_active());
	lv_obj_remove_flag(status_sheet, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_size(status_sheet, screen_width, screen_height);
	lv_obj_set_style_bg_color(status_sheet, lv_color_hex(0x020617), 0);
	lv_obj_set_style_bg_opa(status_sheet, LV_OPA_80, 0);
	lv_obj_set_style_border_width(status_sheet, 0, 0);
	lv_obj_set_style_pad_all(status_sheet, 0, 0);

	card = make_box(status_sheet, 42, 48, screen_width - 84, 160, 0x172033, 18);
	lv_obj_set_style_shadow_width(card, 14, 0);
	lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), 0);
	lv_obj_set_style_shadow_opa(card, LV_OPA_70, 0);
	make_label(card, "SYSTEM STATUS", &lv_font_montserrat_20, 0xf8fafc, 16, 14);

	close_button = lv_button_create(card);
	lv_obj_set_pos(close_button, screen_width - 84 - 42, 10);
	lv_obj_set_size(close_button, 30, 30);
	lv_obj_set_style_radius(close_button, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_border_width(close_button, 0, 0);
	lv_obj_set_style_bg_color(close_button, lv_color_hex(0x334155), 0);
	lv_obj_set_style_pad_all(close_button, 0, 0);
	lv_obj_add_event_cb(close_button, close_status_sheet, LV_EVENT_CLICKED, NULL);
	close_label = lv_label_create(close_button);
	lv_label_set_text(close_label, LV_SYMBOL_CLOSE);
	lv_obj_set_style_text_font(close_label, &lv_font_montserrat_14, 0);
	lv_obj_center(close_label);

	(void)snprintf(summary, sizeof(summary),
		       "Panel   EP4303B  |  %ld x %ld  |  58 Hz\n"
		       "Output  fbdev /dev/fb0\n"
		       "Touch   %s\n\n"
		       "Swipe left or right to change pages.",
		       (long)screen_width, (long)screen_height,
		       touch != NULL ? "GT911 READY" : "GT911 WAITING");
	body = make_label(card, summary, &lv_font_montserrat_14, 0xcbd5e1, 16, 52);
	lv_obj_set_width(body, screen_width - 116);
	lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
}

static void update_page_indicator(lv_event_t *event)
{
	lv_obj_t *active;

	(void)event;
	if (page_indicator == NULL || tileview == NULL)
		return;

	active = lv_tileview_get_tile_active(tileview);
	if (active == tiles[MENU_DISPLAY])
		lv_label_set_text(page_indicator, LV_SYMBOL_LEFT "  2 / 3  " LV_SYMBOL_RIGHT);
	else if (active == tiles[MENU_TOUCH])
		lv_label_set_text(page_indicator, LV_SYMBOL_LEFT "  3 / 3  " LV_SYMBOL_RIGHT);
	else
		lv_label_set_text(page_indicator, LV_SYMBOL_LEFT "  1 / 3  " LV_SYMBOL_RIGHT);
}

static void menu_card_clicked(lv_event_t *event)
{
	uintptr_t target = (uintptr_t)lv_event_get_user_data(event);

	if (target == MENU_BENCHMARK) {
		schedule_benchmark(event);
		return;
	}
	if (target == MENU_RESET_TOUCH) {
		reset_touch_targets();
		lv_tileview_set_tile_by_index(tileview, MENU_TOUCH, 0, LV_ANIM_ON);
		return;
	}
	if (target == MENU_STATUS) {
		show_status_sheet();
		return;
	}

	if (target <= MENU_TOUCH)
		lv_tileview_set_tile_by_index(tileview, (uint32_t)target, 0, LV_ANIM_ON);
}

static void create_launcher_status_bar(lv_obj_t *page)
{
	lv_obj_t *right_status;

	make_label(page, "LCPI", &lv_font_montserrat_14, 0xf8fafc, 12, 3);
	make_label(page, "T113-S3", &lv_font_montserrat_14, 0x94a3b8, 53, 3);
	right_status = make_label(page, LV_SYMBOL_SD_CARD "  FBDEV SAFE",
				  &lv_font_montserrat_14, 0xcbd5e1, 0, 0);
	lv_obj_align(right_status, LV_ALIGN_TOP_RIGHT, -12, 3);
	(void)make_box(page, 0, 21, screen_width, 1, 0x25314a, 0);
}

static lv_obj_t *make_launcher_icon(lv_obj_t *parent, int32_t x, int32_t y,
				    int32_t width, const char *icon,
				    const char *title, uint32_t color, uintptr_t target)
{
	const int32_t icon_size = 48;
	lv_obj_t *button = lv_button_create(parent);
	lv_obj_t *icon_box;
	lv_obj_t *icon_label;
	lv_obj_t *title_label;

	lv_obj_set_pos(button, x, y);
	lv_obj_set_size(button, width, 64);
	lv_obj_set_style_radius(button, 16, 0);
	lv_obj_set_style_border_width(button, 0, 0);
	lv_obj_set_style_bg_opa(button, LV_OPA_TRANSP, 0);
	lv_obj_set_style_bg_color(button, lv_color_hex(0x334155), LV_STATE_PRESSED);
	lv_obj_set_style_bg_opa(button, LV_OPA_50, LV_STATE_PRESSED);
	lv_obj_set_style_pad_all(button, 0, 0);
	lv_obj_add_event_cb(button, menu_card_clicked, LV_EVENT_CLICKED,
			    (void *)target);

	icon_box = make_box(button, (width - icon_size) / 2, 0, icon_size,
			    icon_size, color, 16);
	lv_obj_set_style_shadow_width(icon_box, 7, 0);
	lv_obj_set_style_shadow_color(icon_box, lv_color_hex(color), 0);
	lv_obj_set_style_shadow_opa(icon_box, LV_OPA_50, 0);

	icon_label = lv_label_create(icon_box);
	lv_label_set_text(icon_label, icon);
	lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_26, 0);
	lv_obj_set_style_text_color(icon_label, lv_color_hex(0xffffff), 0);
	lv_obj_center(icon_label);

	title_label = lv_label_create(button);
	lv_label_set_text(title_label, title);
	lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
	lv_obj_set_style_text_color(title_label, lv_color_hex(0xffffff), 0);
	lv_obj_set_width(title_label, width);
	lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_align(title_label, LV_ALIGN_BOTTOM_MID, 0, 0);

	return button;
}

static void create_home_page(lv_obj_t *page)
{
	lv_obj_t *header;
	lv_obj_t *status_panel;
	const int32_t margin = 22;
	const int32_t gap = 20;
	const int32_t icon_width = (screen_width - (margin * 2) - (gap * 2)) / 3;
	const int32_t x0 = margin;
	const int32_t x1 = x0 + icon_width + gap;
	const int32_t x2 = x1 + icon_width + gap;

	create_launcher_status_bar(page);
	header = make_box(page, 12, 29, screen_width - 24, 34, 0x17233a, 15);
	make_label(header, "LCPI Launcher", &lv_font_montserrat_20, 0xf8fafc, 14, 3);
	make_label(header, "EP4303B  |  480 x 272  |  58 Hz",
		   &lv_font_montserrat_14, 0x94a3b8, 176, 10);

	(void)make_launcher_icon(page, x0, 72, icon_width, LV_SYMBOL_IMAGE,
				 "Screen", 0x2563eb, MENU_DISPLAY);
	(void)make_launcher_icon(page, x1, 72, icon_width, LV_SYMBOL_TINT,
				 "Colors", 0xea580c, MENU_DISPLAY);
	(void)make_launcher_icon(page, x2, 72, icon_width, LV_SYMBOL_GPS,
				 "Touch", 0x7c3aed, MENU_TOUCH);
	(void)make_launcher_icon(page, x0, 143, icon_width, LV_SYMBOL_CHARGE,
				 "Benchmark", 0x059669, MENU_BENCHMARK);
	(void)make_launcher_icon(page, x1, 143, icon_width, LV_SYMBOL_REFRESH,
				 "Reset touch", 0x0ea5e9, MENU_RESET_TOUCH);
	(void)make_launcher_icon(page, x2, 143, icon_width, LV_SYMBOL_SETTINGS,
				 "Status", 0xd946ef, MENU_STATUS);

	status_panel = make_box(page, 12, 218, screen_width - 24, 31, 0x172033, 12);
	display_status_home = make_label(status_panel, "DISPLAY: starting...",
					 &lv_font_montserrat_14, 0xffffff, 10, 3);
	touch_status_home = make_label(status_panel, "GT911: WAITING",
					&lv_font_montserrat_14, 0xff6b6b, 10, 17);
}

static void create_color_pattern(lv_obj_t *page)
{
	static const uint32_t colors[] = {
		0xff3030, 0x28d17c, 0x398bff, 0xffffff,
		0x00d5e8, 0xc06cff, 0xffcc33,
	};
	const int32_t bar_y = 56;
	const int32_t bar_height = 35;
	const int32_t bar_width = screen_width / (int32_t)(sizeof(colors) / sizeof(colors[0]));
	const int32_t checker_y = 102;
	const int32_t checker_rows = 5;
	const int32_t checker_cols = 12;
	const int32_t checker_height = 82;
	const int32_t cell_width = screen_width / checker_cols;
	const int32_t cell_height = checker_height / checker_rows;
	size_t index;
	int row;
	int col;

	make_label(page, "DISPLAY TEST", &lv_font_montserrat_20, 0xffffff, 12, 8);
	make_label(page, "RGB / gray / motion", &lv_font_montserrat_14,
		   0x94a3b8, 14, 32);

	for (index = 0; index < sizeof(colors) / sizeof(colors[0]); ++index) {
		int32_t x = (int32_t)index * bar_width;
		int32_t width = (index + 1 == sizeof(colors) / sizeof(colors[0])) ?
			screen_width - x : bar_width;
		(void)make_box(page, x, bar_y, width, bar_height, colors[index], 0);
	}

	for (row = 0; row < checker_rows; ++row) {
		for (col = 0; col < checker_cols; ++col) {
			int32_t x = col * cell_width;
			int32_t y = checker_y + row * cell_height;
			int32_t width = (col + 1 == checker_cols) ?
				screen_width - x : cell_width;
			int32_t height = (row + 1 == checker_rows) ?
				checker_height - row * cell_height : cell_height;
			(void)make_box(page, x, y, width, height,
				       ((row + col) & 1) ? 0x202938 : 0xd6deea, 0);
		}
	}

	moving_block = make_box(page, 0, 201, 22, 9, 0xffd166, 5);
	display_status_page = make_label(page, "DISPLAY: starting...",
					 &lv_font_montserrat_14, 0xffffff, 12, 220);
	make_label(page, "Motion bar should move smoothly", &lv_font_montserrat_14,
		   0x94a3b8, 12, 240);
}

static void create_touch_targets(lv_obj_t *page)
{
	static const int32_t target_x[TARGET_COUNT] = {20, 430, 20, 430, 225};
	static const int32_t target_y[TARGET_COUNT] = {118, 118, 183, 183, 150};
	const int32_t target_size = 30;
	unsigned int index;

	for (index = 0; index < TARGET_COUNT; ++index) {
		lv_obj_t *button = lv_button_create(page);
		lv_obj_t *label;

		lv_obj_set_pos(button, target_x[index], target_y[index]);
		lv_obj_set_size(button, target_size, target_size);
		lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, 0);
		lv_obj_set_style_bg_color(button, lv_color_hex(0xdc2626), 0);
		lv_obj_set_style_border_width(button, 2, 0);
		lv_obj_set_style_border_color(button, lv_color_hex(0xffffff), 0);
		lv_obj_add_event_cb(button, target_clicked, LV_EVENT_CLICKED,
				    (void *)(uintptr_t)index);

		label = lv_label_create(button);
		lv_label_set_text(label, "+");
		lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
		lv_obj_center(label);
		targets[index] = button;
	}

	cursor = lv_obj_create(page);
	lv_obj_remove_flag(cursor, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_clear_flag(cursor, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_size(cursor, 11, 11);
	lv_obj_set_style_bg_opa(cursor, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(cursor, 2, 0);
	lv_obj_set_style_border_color(cursor, lv_color_hex(0x38bdf8), 0);
	lv_obj_set_style_radius(cursor, LV_RADIUS_CIRCLE, 0);
	lv_obj_add_flag(cursor, LV_OBJ_FLAG_HIDDEN);
}

static void create_touch_page(lv_obj_t *page)
{
	lv_obj_t *button;

	make_label(page, "TOUCH & PERFORMANCE", &lv_font_montserrat_20,
		   0xffffff, 12, 8);
	touch_status_page = make_label(page, "GT911: WAITING", &lv_font_montserrat_14,
				      0xff6b6b, 14, 42);
	coordinate_label = make_label(page, "Touch: --, --", &lv_font_montserrat_14,
				    0xffffff, 14, 63);
	touch_result = make_label(page, "Touch targets: 0/5", &lv_font_montserrat_14,
				  0xffffff, 14, 84);
	create_touch_targets(page);

	button = lv_button_create(page);
	lv_obj_set_pos(button, 118, 221);
	lv_obj_set_size(button, 244, 29);
	lv_obj_set_style_radius(button, 10, 0);
	lv_obj_set_style_bg_color(button, lv_color_hex(0x059669), 0);
	lv_obj_set_style_border_width(button, 0, 0);
	lv_obj_add_event_cb(button, schedule_benchmark, LV_EVENT_CLICKED, NULL);
	benchmark_button_label = lv_label_create(button);
	lv_label_set_text(benchmark_button_label, LV_SYMBOL_PLAY "  RUN LVGL BENCHMARK");
	lv_obj_set_style_text_font(benchmark_button_label, &lv_font_montserrat_14, 0);
	lv_obj_center(benchmark_button_label);
}

static void create_ui(void)
{
	lv_obj_t *screen = lv_screen_active();
	unsigned int page_index;

	screen_width = lv_display_get_horizontal_resolution(display);
	screen_height = lv_display_get_vertical_resolution(display);
	lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_bg_color(screen, lv_color_hex(0x0b1020), 0);
	lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
	lv_obj_set_style_pad_all(screen, 0, 0);

	tileview = lv_tileview_create(screen);
	lv_obj_set_scrollbar_mode(tileview, LV_SCROLLBAR_MODE_OFF);
	lv_obj_set_style_bg_opa(tileview, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(tileview, 0, 0);
	lv_obj_set_style_pad_all(tileview, 0, 0);

	tiles[MENU_HOME] = lv_tileview_add_tile(tileview, MENU_HOME, 0, LV_DIR_HOR);
	tiles[MENU_DISPLAY] = lv_tileview_add_tile(tileview, MENU_DISPLAY, 0, LV_DIR_HOR);
	tiles[MENU_TOUCH] = lv_tileview_add_tile(tileview, MENU_TOUCH, 0, LV_DIR_HOR);
	for (page_index = 0; page_index < 3; ++page_index) {
		lv_obj_set_style_bg_color(tiles[page_index], lv_color_hex(0x0b1020), 0);
		lv_obj_set_style_bg_opa(tiles[page_index], LV_OPA_COVER, 0);
		lv_obj_set_style_border_width(tiles[page_index], 0, 0);
		lv_obj_set_style_pad_all(tiles[page_index], 0, 0);
	}
	create_home_page(tiles[MENU_HOME]);
	create_color_pattern(tiles[MENU_DISPLAY]);
	create_touch_page(tiles[MENU_TOUCH]);
	lv_tileview_set_tile(tileview, tiles[MENU_HOME], LV_ANIM_OFF);
	lv_obj_add_event_cb(tileview, update_page_indicator, LV_EVENT_VALUE_CHANGED, NULL);

	page_indicator = make_label(screen, LV_SYMBOL_LEFT "  1 / 3  " LV_SYMBOL_RIGHT,
				    &lv_font_montserrat_14, 0x94a3b8, 0, 0);
	lv_obj_align(page_indicator, LV_ALIGN_BOTTOM_MID, 0, -3);
	lv_obj_remove_flag(page_indicator, LV_OBJ_FLAG_CLICKABLE);

	update_display_status();
	set_touch_status("GT911: WAITING", 0xff6b6b);
	update_touch_result();

	touch_scan_timer = lv_timer_create(scan_touch_timer, 1000, NULL);
	touch_feedback_timer = lv_timer_create(update_touch_feedback, 17, NULL);
	refresh_marker_timer = lv_timer_create(move_refresh_marker, 17, NULL);
}

static lv_display_t *try_open_fbdev(void)
{
	lv_display_t *new_display;

	if (access("/dev/fb0", R_OK | W_OK) != 0)
		return NULL;

	new_display = lv_linux_fbdev_create();
	if (new_display != NULL &&
	    lv_linux_fbdev_set_file(new_display, "/dev/fb0") == LV_RESULT_OK)
		return new_display;

	if (new_display != NULL)
		lv_display_delete(new_display);
	return NULL;
}

static lv_display_t *open_display(void)
{
	lv_display_t *new_display;

	while (!stop_requested) {
		new_display = try_open_fbdev();
		if (new_display != NULL)
			return new_display;

		(void)sleep(1);
	}

	return NULL;
}

int main(void)
{
	struct sigaction action;

	memset(&action, 0, sizeof(action));
	action.sa_handler = signal_handler;
	(void)sigaction(SIGINT, &action, NULL);
	(void)sigaction(SIGTERM, &action, NULL);

	lv_init();
	display = open_display();
	if (display == NULL)
		return EXIT_FAILURE;

	hide_framebuffer_console();
	create_ui();
	try_attach_goodix();

	while (!stop_requested) {
		uint32_t wait_ms = lv_timer_handler();

		/* Keep a 17 ms refresh cadence without the old 5 ms quantisation. */
		if (wait_ms == LV_NO_TIMER_READY || wait_ms > 17)
			wait_ms = 17;
		if (wait_ms == 0)
			wait_ms = 1;
		(void)usleep(wait_ms * 1000U);
	}

	restore_framebuffer_console();
	return EXIT_SUCCESS;
}
