#ifndef LVGLDS
#define LVGLDS

/// Configuration options (comment to disable)

#define LVDS_INIT_LOWER_DISPLAY
#define LVDS_INIT_UPPER_DISPLAY

#define LVDS_USE_FB_MODE_FOR_MAIN
#define LVDS_MAIN_ENGINE_ON_BOTTOM

#define LVDS_MAIN_ENGINE_LAYER 3
#define LVDS_SUB_ENGINE_LAYER  3

#include <../lvgl/lvgl.h>

/// API

/// Pointers (usually into VRAM) to ARGB555 16bpp bitmaps which the library renders into.
extern uint16_t * lvds_screen_upper_bmp;
extern uint16_t * lvds_screen_lower_bmp;

extern lv_display_t * lvds_upper_display;
extern lv_display_t * lvds_lower_display;

extern lv_group_t * lvds_keypad_group;

/// Initialises LVGL and the DS hardware.
void lvds_init(void);

/// Initialises DS hardware for use with lvgl-ds.
///
/// This function initialises DS rendering according to the configuration
/// options above. By default, it initialises the main engine to framebuffer
/// mode on VRAM_B and the sub engine to Mode 5 with LVGL rendering on 
/// a 16bpp 256x256 bitmap at layer 3, stored in VRAM_C. 
/// The touch screen is set to utilise the main engine.
/// The buttons are connected to `lvgl_keypad_group` which is set as the default one.
///
void lvds_init_ds(void);

/// Initialises LVGL and DS drivers.
///
/// `lower_bmp` and `upper_bmp` should point to ARGB555 bitmaps or 
/// framebuffers in VRAM at least 16bpp 256x192 in size.
/// Initialises both LVGL displays, the touchscreen, keypad, timer system etc.
/// Internally call `lv_init`, there is no need to call it separately.
void lvds_init_lvgl(uint16_t * lower_bmp, uint16_t * upper_bmp);

/// Performs an LVGL update. 
///
/// To be called once a screen refresh (every 1/60th of a second).
/// In most cases, simply call this function once in the main loop of your program.
/// This updates internal input and timer data and then calls `lv_timer_periodic_handler`.
void lvds_update(void);

/// Rotate the screen and remap buttons for easier use.
/// - `lvds_set_orientation_hoz` sets the default, horizontal orientation.
/// - `lvds_set_orientation_vertl` sets a vertical orientation with the upper
///    (non-touch) screen on the left. ("right-handed vertical mode")
/// - `lvds_set_orientation_vertr` sets a vertical orientation with the upper 
///    (non-touch) screen on the right. ("left-handed vertical mode") 
void lvds_set_orientation_hoz(void);
void lvds_set_orientation_vertl(void);
void lvds_set_orientation_vertr(void);

#endif
