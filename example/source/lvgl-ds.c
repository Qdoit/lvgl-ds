#include "lvgl-ds.h"
#include <nds.h>

uint16_t * lvds_screen_upper_bmp;
uint16_t * lvds_screen_lower_bmp;

lv_display_t * lvds_upper_display;
lv_display_t * lvds_lower_display;

lv_group_t * lvds_keypad_group;

// Tracks the amount of screen refreshes since program start
// Used for calculating ticks elapsed.
static uint32_t ELAPSED_VBLANKS = 0;

static uint16_t KEYS_BUFFER = 0;

static lv_indev_t * keypad_indev;
static lv_indev_t * touchscreen_indev;

// built-in rotation primitives require at least max_side^2 buffer size
#define LVGL_BUFFER_SIZE 256*256*2
static uint8_t lvgl_buf1[LVGL_BUFFER_SIZE];
static uint8_t lvgl_buf2[LVGL_BUFFER_SIZE];

uint32_t get_miliseconds_elapsed();

void flush_cb_upper(lv_display_t *, const lv_area_t *, uint8_t *);
void flush_cb_lower(lv_display_t *, const lv_area_t *, uint8_t *);

void flush_cb_upper_vertl(lv_display_t *, const lv_area_t *, uint8_t *);
void flush_cb_lower_vertl(lv_display_t *, const lv_area_t *, uint8_t *);
void flush_cb_upper_vertr(lv_display_t *, const lv_area_t *, uint8_t *);
void flush_cb_lower_vertr(lv_display_t *, const lv_area_t *, uint8_t *);

void  touchscreen_read(lv_indev_t *, lv_indev_data_t *);
void       keypad_read(lv_indev_t *, lv_indev_data_t *);
void keypad_read_vertl(lv_indev_t *, lv_indev_data_t *);
void keypad_read_vertr(lv_indev_t *, lv_indev_data_t *);

uint16_t * init_main_engine();
uint16_t * init_sub_engine();

void lvds_init(void) {
    lvds_init_ds();
    lvds_init_lvgl(lvds_screen_lower_bmp, lvds_screen_upper_bmp);
}

void lvds_update(void) {
    scanKeys();
    KEYS_BUFFER |= keysCurrent();
    ELAPSED_VBLANKS++;
    lv_timer_periodic_handler();
}

void lvds_init_ds(void) {
    #ifdef LVDS_MAIN_ENGINE_ON_BOTTOM
        lcdMainOnBottom();
        #ifdef LVDS_INIT_LOWER_DISPLAY
            lvds_screen_lower_bmp = init_main_engine();
        #endif
        #ifdef LVDS_INIT_UPPER_DISPLAY
            lvds_screen_upper_bmp = init_sub_engine();
        #endif
    #else
        #ifdef LVDS_INIT_LOWER_DISPLAY
            lvds_screen_lower_bmp = init_sub_engine();
        #endif
        #ifdef LVDS_INIT_UPPER_DISPLAY
            lvds_screen_upper_bmp = init_main_engine();
        #endif
    #endif
}

void lvds_init_lvgl(uint16_t * lower_bmp, uint16_t * upper_bmp) {
    lvds_screen_lower_bmp = lower_bmp;
    lvds_screen_upper_bmp = upper_bmp;

    lv_init();

    // Initialise tick interface
    lv_tick_set_cb(get_miliseconds_elapsed);
   
    #ifdef LVDS_INIT_UPPER_DISPLAY
        lv_display_t * upper_display = lv_display_create(256, 192);
        lv_display_set_buffers(upper_display, lvgl_buf1, NULL, 
                               LVGL_BUFFER_SIZE, LV_DISPLAY_RENDER_MODE_DIRECT);
        lv_display_set_flush_cb(upper_display, flush_cb_upper);
        lvds_upper_display = upper_display;
    #endif

    #ifdef LVDS_INIT_LOWER_DISPLAY
        lv_display_t * lower_display = lv_display_create(256, 192);
        lv_display_set_buffers(lower_display, lvgl_buf2, NULL, 
                               LVGL_BUFFER_SIZE, LV_DISPLAY_RENDER_MODE_DIRECT);
        lv_display_set_flush_cb(lower_display, flush_cb_lower);
        lvds_lower_display = lower_display;

        // Initialise touchpad, connecting it to lower_display.
        lv_display_set_default(lower_display);
        
        touchscreen_indev = lv_indev_create();
        lv_indev_set_type(touchscreen_indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(touchscreen_indev, touchscreen_read);
    #endif

    keypad_indev = lv_indev_create();
    lv_indev_set_type(keypad_indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(keypad_indev, keypad_read);
    lvds_keypad_group = lv_group_create();
    lv_group_set_default(lvds_keypad_group);
    lv_indev_set_group(keypad_indev, lvds_keypad_group);
}

uint16_t * init_main_engine() {
#ifdef LVDS_USE_FB_MODE_FOR_MAIN
    videoSetMode(MODE_FB1);
    vramSetBankB(VRAM_B_LCD);
    return VRAM_B;
#else
    videoSetMode(MODE_5_2D);
    vramSetBankB(VRAM_B_MAIN_BG);
    int main_bg_id = bgInit(LVDS_MAIN_ENGINE_LAYER, BgType_Bmp16, 
                            BgSize_B16_256x256, 0, 0);
    return bgGetGfxPtr(main_bg_id);
#endif
}

uint16_t * init_sub_engine() {
    videoSetModeSub(MODE_5_2D);
    vramSetBankC(VRAM_C_SUB_BG);
    int sub_bg_id = bgInitSub(LVDS_SUB_ENGINE_LAYER, BgType_Bmp16,
                              BgSize_B16_256x256, 16, 0);
    return bgGetGfxPtr(sub_bg_id);
}

uint32_t get_miliseconds_elapsed() {
    // A refresh occurs once every 16 + (2/3) miliseconds
    return (ELAPSED_VBLANKS<<4) + (ELAPSED_VBLANKS<<1) / (ELAPSED_VBLANKS*3);
}

// Display flush callbacks

inline uint16_t convert_color(uint16_t color) {
    uint16_t red   = color       & 0b11111;
    uint16_t green = color >> 6  & 0b11111;
    uint16_t blue  = color >> 11 & 0b11111;

    return BIT(15) | red << 10 | green << 5 | blue;
}

// Generic flush function for both screens.
// Converts BGR565 to ARGB555 and writes the result to the right location in the `bmp` destination parameter. 
void flush_cb_with_bmp_dst(uint16_t * bmp, lv_display_t * display, 
                           const lv_area_t * area, uint8_t * px_map) {
    uint16_t * buf = (uint16_t*)px_map;
    for(int y = area->y1; y <= area->y2; y++) {
        for(int x = area->x1; x <= area->x2; x++) {
            uint16_t color = convert_color(buf[y*256 + x]);
            bmp[y*256 + x] = color; 
        }
    }

    lv_display_flush_ready(display);
}

void flush_cb_upper(lv_display_t * display, const lv_area_t * area, uint8_t * px_map) {
    flush_cb_with_bmp_dst(lvds_screen_upper_bmp, display, area, px_map);
}

void flush_cb_lower(lv_display_t * display, const lv_area_t * area, uint8_t * px_map) {
    flush_cb_with_bmp_dst(lvds_screen_lower_bmp, display, area, px_map);
}

// Input Devices

void touchscreen_read(lv_indev_t * indev, lv_indev_data_t * data) {
    if(keysCurrent() & KEY_TOUCH) {
        touchPosition pos;
        touchRead(&pos);
        data->point.x = pos.px;
        data->point.y = pos.py;
        data->state = LV_INDEV_STATE_PRESSED;
        KEYS_BUFFER &= !KEY_TOUCH;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void keypad_read(lv_indev_t * indev, lv_indev_data_t * data) {
    const int16_t used_keys = KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN 
                              | KEY_R | KEY_L | KEY_A | KEY_B 
                              | KEY_X | KEY_Y;
    if(KEYS_BUFFER & used_keys) {
        data->state = LV_INDEV_STATE_PRESSED;
        if(KEYS_BUFFER & KEY_LEFT) 
            data->key = LV_KEY_LEFT;
        else if(KEYS_BUFFER & KEY_RIGHT) 
            data->key = LV_KEY_RIGHT;
        else if(KEYS_BUFFER & KEY_UP) 
            data->key = LV_KEY_UP;
        else if(KEYS_BUFFER & KEY_DOWN) 
            data->key = LV_KEY_DOWN;
        else if(KEYS_BUFFER & KEY_L) 
            data->key = LV_KEY_PREV;
        else if(KEYS_BUFFER & KEY_R) 
            data->key = LV_KEY_NEXT;
        else if(KEYS_BUFFER & KEY_A) 
            data->key = LV_KEY_ENTER;
        else if(KEYS_BUFFER & KEY_B) 
            data->key = LV_KEY_ESC;
        else if(KEYS_BUFFER & KEY_X) 
            data->key = LV_KEY_HOME;
        else if(KEYS_BUFFER & KEY_Y) 
            data->key = LV_KEY_BACKSPACE;
        
        KEYS_BUFFER &= !used_keys;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }

}

// Rotation functions

void lvds_set_orientation_hoz(void) {
    if(lvds_lower_display != 0) {
        lv_display_set_rotation(lvds_lower_display, LV_DISPLAY_ROTATION_0);
    lv_display_set_flush_cb(lvds_lower_display, flush_cb_lower);
    }
    if(lvds_upper_display != 0) {
        lv_display_set_rotation(lvds_upper_display, LV_DISPLAY_ROTATION_0);
        lv_display_set_flush_cb(lvds_upper_display, flush_cb_upper);
    }
    lv_indev_set_read_cb(keypad_indev, keypad_read);
}

void lvds_set_orientation_vertr(void) {
    if(lvds_lower_display != 0) {
        lv_display_set_rotation(lvds_lower_display, LV_DISPLAY_ROTATION_90);
        lv_display_set_flush_cb(lvds_lower_display, flush_cb_lower_vertr);
    }
    if(lvds_upper_display != 0) {
        lv_display_set_rotation(lvds_upper_display, LV_DISPLAY_ROTATION_90);
        lv_display_set_flush_cb(lvds_upper_display, flush_cb_upper_vertr);
    }
    lv_indev_set_read_cb(keypad_indev, keypad_read_vertr);
}

void lvds_set_orientation_vertl(void) {
    if(lvds_lower_display != 0) {
        lv_display_set_rotation(lvds_lower_display, LV_DISPLAY_ROTATION_270);
        lv_display_set_flush_cb(lvds_lower_display, flush_cb_lower_vertl);
    }
    if(lvds_upper_display != 0) {
        lv_display_set_rotation(lvds_upper_display, LV_DISPLAY_ROTATION_270);
        lv_display_set_flush_cb(lvds_upper_display, flush_cb_upper_vertl);
    }

    lv_indev_set_read_cb(keypad_indev, keypad_read_vertl);
}


// Rotation callbacks (mostly minorly modified variants of earlier functions)

void flush_cb_with_bmp_dst_vertr(uint16_t * bmp, lv_display_t * display, 
                                 const lv_area_t * area, uint8_t * px_map) {
    uint16_t * buf = (uint16_t*)px_map;
    for(int y = area->y1; y <= area->y2; y++) {
        for(int x = area->x1; x <= area->x2; x++) {
            uint16_t color = convert_color(buf[y*256 + x]);
            bmp[(191-x)*256 + y] = color;
        }
    }

    lv_display_flush_ready(display);
}

void flush_cb_with_bmp_dst_vertl(uint16_t * bmp, lv_display_t * display, 
                                 const lv_area_t * area, uint8_t * px_map) {
    uint16_t * buf = (uint16_t*)px_map;
    for(int y = area->y1; y <= area->y2; y++) {
        for(int x = area->x1; x <= area->x2; x++) {
            uint16_t color = convert_color(buf[y*256 + x]);
            bmp[x*256 + (255-y)] = color;
        }
    }

    lv_display_flush_ready(display);
}

void flush_cb_upper_vertl(lv_display_t * display, const lv_area_t * area, uint8_t * px_map) {
    flush_cb_with_bmp_dst_vertl(lvds_screen_upper_bmp, display, area, px_map);
}

void flush_cb_lower_vertl(lv_display_t * display, const lv_area_t * area, uint8_t * px_map) {
    flush_cb_with_bmp_dst_vertl(lvds_screen_lower_bmp, display, area, px_map);
}

void flush_cb_upper_vertr(lv_display_t * display, const lv_area_t * area, uint8_t * px_map) {
    flush_cb_with_bmp_dst_vertr(lvds_screen_upper_bmp, display, area, px_map);
}

void flush_cb_lower_vertr(lv_display_t * display, const lv_area_t * area, uint8_t * px_map) {
    flush_cb_with_bmp_dst_vertr(lvds_screen_lower_bmp, display, area, px_map);
}

void keypad_read_vertl(lv_indev_t * indev, lv_indev_data_t * data) {
    const int16_t used_keys = KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN 
                              | KEY_R | KEY_L | KEY_A | KEY_B 
                              | KEY_X | KEY_Y;
    if(KEYS_BUFFER & used_keys) {
        data->state = LV_INDEV_STATE_PRESSED;
        if(KEYS_BUFFER & KEY_LEFT) 
            data->key = LV_KEY_DOWN;
        else if(KEYS_BUFFER & KEY_RIGHT) 
            data->key = LV_KEY_UP;
        else if(KEYS_BUFFER & KEY_UP) 
            data->key = LV_KEY_PREV;
        else if(KEYS_BUFFER & KEY_DOWN) 
            data->key = LV_KEY_NEXT;
        else if(KEYS_BUFFER & KEY_L) 
            data->key = LV_KEY_ENTER;
        else if(KEYS_BUFFER & KEY_R) 
            data->key = LV_KEY_ENTER;
        else if(KEYS_BUFFER & KEY_A) 
            data->key = LV_KEY_ESC;
        else if(KEYS_BUFFER & KEY_B) 
            data->key = LV_KEY_ENTER;
        else if(KEYS_BUFFER & KEY_X) 
            data->key = LV_KEY_LEFT;
        else if(KEYS_BUFFER & KEY_Y) 
            data->key = LV_KEY_RIGHT;
        
        KEYS_BUFFER &= !used_keys;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void keypad_read_vertr(lv_indev_t * indev, lv_indev_data_t * data) {
    const int16_t used_keys = KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN 
                              | KEY_R | KEY_L | KEY_A | KEY_B 
                              | KEY_X | KEY_Y;
    if(KEYS_BUFFER & used_keys) {
        data->state = LV_INDEV_STATE_PRESSED;
        if(KEYS_BUFFER & KEY_B) 
            data->key = LV_KEY_PREV;
        else if(KEYS_BUFFER & KEY_X) 
            data->key = LV_KEY_NEXT;
        else if(KEYS_BUFFER & KEY_Y) 
            data->key = LV_KEY_UP;
        else if(KEYS_BUFFER & KEY_A) 
            data->key = LV_KEY_DOWN;
        else if(KEYS_BUFFER & KEY_R) 
            data->key = LV_KEY_ENTER;
        else if(KEYS_BUFFER & KEY_L) 
            data->key = LV_KEY_ENTER;
        else if(KEYS_BUFFER & KEY_UP) 
            data->key = LV_KEY_ENTER;
        else if(KEYS_BUFFER & KEY_LEFT) 
            data->key = LV_KEY_ESC;
        else if(KEYS_BUFFER & KEY_DOWN) 
            data->key = LV_KEY_LEFT;
        else if(KEYS_BUFFER & KEY_RIGHT) 
            data->key = LV_KEY_RIGHT;
        
        KEYS_BUFFER &= !used_keys;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}


