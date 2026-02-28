#include "ui.h"
#include <lvgl.h>

LV_FONT_DECLARE(lv_font_montserrat_24);
LV_FONT_DECLARE(lv_font_montserrat_32);
LV_FONT_DECLARE(lv_font_montserrat_48);

// Screens
static lv_obj_t *scr_config;
static lv_obj_t *scr_connecting;
static lv_obj_t *scr_nav;

// Nav screen components declaration
static lv_obj_t *lbl_maneuver;
static lv_obj_t *lbl_distance;
static lv_obj_t *lbl_street;

// Button Event callback to switch to connecting screen
static void btn_pair_evt(lv_event_t *e) { ui_show_connecting(); }

void ui_init() {
  // 1. Config Screen Initialization
  scr_config = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr_config, lv_color_black(), 0);

  lv_obj_t *title_cfg = lv_label_create(scr_config);
  lv_label_set_text(title_cfg, "Configuration");
  lv_obj_set_style_text_color(title_cfg, lv_color_white(), 0);
  lv_obj_align(title_cfg, LV_ALIGN_TOP_MID, 0, 30);

  lv_obj_t *btn_pair = lv_btn_create(scr_config);
  lv_obj_align(btn_pair, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(btn_pair, lv_color_make(50, 50, 50),
                            0); // Dark gray for monochrome style

  lv_obj_t *lbl_btn = lv_label_create(btn_pair);
  lv_label_set_text(lbl_btn, "Pair Phone");
  lv_obj_set_style_text_color(lbl_btn, lv_color_white(), 0);
  lv_obj_add_event_cb(btn_pair, btn_pair_evt, LV_EVENT_CLICKED, NULL);

  // 2. Connecting Screen Initialization
  scr_connecting = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr_connecting, lv_color_black(), 0);
  lv_obj_t *title_conn = lv_label_create(scr_connecting);
  lv_label_set_text(title_conn, "Connecting to\nPhone via BLE...");
  lv_obj_set_style_text_align(title_conn, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(title_conn, lv_color_white(), 0);
  lv_obj_align(title_conn, LV_ALIGN_CENTER, 0, 0);

  // 3. Navigation Screen Initialization
  scr_nav = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr_nav, lv_color_black(), 0);

  lbl_maneuver = lv_label_create(scr_nav);
  lv_obj_set_style_text_font(lbl_maneuver, &lv_font_montserrat_48,
                             0); // Massive icon font
  lv_label_set_text(lbl_maneuver, LV_SYMBOL_UP);
  lv_obj_set_style_text_color(lbl_maneuver, lv_color_white(), 0);
  lv_obj_align(lbl_maneuver, LV_ALIGN_CENTER, 0, -60);

  lbl_distance = lv_label_create(scr_nav);
  lv_obj_set_style_text_font(lbl_distance, &lv_font_montserrat_32,
                             0); // Large distance font
  lv_label_set_text(lbl_distance, "---");
  lv_obj_set_style_text_color(lbl_distance, lv_color_white(), 0);
  lv_obj_align(lbl_distance, LV_ALIGN_CENTER, 0, 0);

  lbl_street = lv_label_create(scr_nav);
  lv_obj_set_style_text_font(lbl_street, &lv_font_montserrat_24,
                             0); // Readable street font
  lv_label_set_text(lbl_street, "Waiting for route...");
  lv_obj_set_style_text_color(lbl_street, lv_color_white(), 0);
  lv_obj_align(lbl_street, LV_ALIGN_CENTER, 0, 50);

  // Show config screen by default
  lv_scr_load(scr_config);
}

void ui_show_config() {
  lv_scr_load_anim(scr_config, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false);
}

void ui_show_connecting() {
  lv_scr_load_anim(scr_connecting, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false);
}

void ui_show_navigation(int maneuverId, const char *distance,
                        const char *street) {
  const char *symbol = LV_SYMBOL_UP; // Default straight

  switch (maneuverId) {
  case 1:
    symbol = LV_SYMBOL_LEFT;
    break;
  case 2:
    symbol = LV_SYMBOL_RIGHT;
    break;
  case 3:
    symbol = LV_SYMBOL_NEW_LINE LV_SYMBOL_LEFT;
    break; // Bear Left (Hack: can just use left if no specific diag)
  case 4:
    symbol = LV_SYMBOL_NEW_LINE LV_SYMBOL_RIGHT;
    break; // Bear Right
  case 5:
    symbol = LV_SYMBOL_REFRESH;
    break; // U-Turn
  case 6:
    symbol = LV_SYMBOL_LOOP;
    break; // Roundabout
  case 7:
    symbol = LV_SYMBOL_HOME;
    break; // Arrive
  default:
    symbol = LV_SYMBOL_UP;
    break; // Straight
  }

  // Fallback for bear left/right if multiline symbols look bad, just use
  // standard vectors
  if (maneuverId == 3)
    symbol = LV_SYMBOL_LEFT;
  if (maneuverId == 4)
    symbol = LV_SYMBOL_RIGHT;

  lv_label_set_text(lbl_maneuver, symbol);
  lv_label_set_text(lbl_distance, distance);
  lv_label_set_text(lbl_street, street);

  // Switch to Navigation screen if not already visible
  if (lv_scr_act() != scr_nav) {
    lv_scr_load_anim(scr_nav, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false);
  }
}
