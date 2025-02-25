#include <fat.h>
#include <dirent.h>
#include <stdio.h>
#include "lvgl.h"
#include <nds.h>
#include "lvgl-ds.h"

static void lv_example_scroll_2(void);
static void btn_event_cb(lv_event_t * e);
static void button_example(void);
static void keyboard_example();

void my_log_cb(lv_log_level_t level, const char * buf)
{
  fprintf(stderr, "%s\n", buf);
}

int main(int argc, char **argv)
{   
    fatInitDefault();
    lvds_init();

    // Setup debug to no$gba console
    consoleDebugInit(DebugDevice_NOCASH);
    fprintf(stderr, "Debug message!\n");
    //lv_log_register_print_cb(my_log_cb);

    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x43346b), LV_PART_MAIN);
    button_example();
    lv_example_scroll_2(); 
    keyboard_example();

    while (1)
    {
        // Synchronize game loop to the screen refresh
        swiWaitForVBlank();

        lvds_update();
        
        // Normal libnds functionality is largely unaffected!
        uint16_t keys = keysHeld();

        if (keys & KEY_START)
            break;
    }

    return 0;
}

static void ta_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * ta = lv_event_get_target(e);
    lv_obj_t * kb = lv_event_get_user_data(e);
    if(code == LV_EVENT_FOCUSED) {
        lv_keyboard_set_textarea(kb, ta);
        lv_obj_remove_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }

    if(code == LV_EVENT_DEFOCUSED) {
        lv_keyboard_set_textarea(kb, NULL);
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }
}

// Modified version of LVGL example. Licensed under MIT with credit to LVGL Kft.
static void keyboard_example() {
    lv_display_set_default(lvds_lower_display);
    lv_obj_t * kb = lv_keyboard_create(lv_screen_active());
    lv_keyboard_set_textarea(kb, NULL);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);

    lv_display_set_default(lvds_upper_display);
    lv_obj_t * ta1 = lv_textarea_create(lv_screen_active());
    lv_obj_set_style_text_font(ta1, &lv_font_unscii_8, LV_PART_MAIN);
    lv_obj_align(ta1, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_add_event_cb(ta1, ta_event_cb, LV_EVENT_ALL, kb);
    lv_textarea_set_placeholder_text(ta1, "Hello");
    lv_obj_set_size(ta1, 140, 80);

    lv_obj_t * ta2 = lv_textarea_create(lv_screen_active());
    lv_obj_align(ta2, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_add_event_cb(ta2, ta_event_cb, LV_EVENT_ALL, kb);
    lv_obj_set_size(ta2, 140, 80);
}

static void btn_event_cb(lv_event_t * e)
{
    static int state = 0;
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        switch(state) {
        case 0:
            lvds_set_orientation_vertl();
            break;
        case 1:
            lvds_set_orientation_vertr();
            break;
        case 2:
            lvds_set_orientation_hoz();
            state = -1;
            break;
        }
        state++;
    }
}

static void button_example(void) {
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x43346b), LV_PART_MAIN);
    lv_obj_set_style_text_font(lv_screen_active(), &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_t * btn = lv_button_create(lv_screen_active());    
    lv_obj_set_pos(btn, 10, 10);
    lv_obj_set_size(btn, 30, 30);        
    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_t * label = lv_label_create(btn);
    lv_label_set_text(label, LV_SYMBOL_REFRESH);              
    lv_obj_center(label);
}

// Modified version of LVGL example. Licensed under MIT with credit to LVGL Kft.
static void lv_example_scroll_2(void)
{
    lv_obj_t * panel = lv_obj_create(lv_screen_active());
    lv_obj_set_size(panel, 192, 120);
    lv_obj_set_scroll_snap_x(panel, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_ROW);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 20);

    uint32_t i;
    for(i = 0; i < 10; i++) {
        lv_obj_t * btn = lv_button_create(panel);
        lv_obj_set_size(btn, 150, lv_pct(100));

        lv_obj_t * label = lv_label_create(btn);
        if(i == 3) {
            lv_label_set_text_fmt(label, "Panel %"LV_PRIu32"\nno snap", i);
            lv_obj_remove_flag(btn, LV_OBJ_FLAG_SNAPPABLE);
        }
        else {
            lv_label_set_text_fmt(label, "Panel %"LV_PRIu32, i);
        }

        lv_obj_center(label);
    }
    lv_obj_update_snap(panel, LV_ANIM_ON);
}
