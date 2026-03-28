#include "ui.h"
#include <cstdio>
#include <lvgl.h>

LV_FONT_DECLARE(lv_font_montserrat_24);
LV_FONT_DECLARE(lv_font_montserrat_32);
LV_FONT_DECLARE(lv_font_montserrat_48);

LV_IMG_DECLARE(ui_img_ic_straight_png);
LV_IMG_DECLARE(ui_img_ic_turn_left_png);
LV_IMG_DECLARE(ui_img_ic_turn_right_png);
LV_IMG_DECLARE(ui_img_ic_turn_slight_left_png);
LV_IMG_DECLARE(ui_img_ic_turn_slight_right_png);
LV_IMG_DECLARE(ui_img_ic_turn_u_turn_counterclockwise_png);
LV_IMG_DECLARE(ui_img_ic_roundabout_clockwise_png);
LV_IMG_DECLARE(ui_img_ic_destination_png);

// ---------------------------------------------------------------------------
// Screens  (cycle: Nav → Arrival → Clock → Nav)
// ---------------------------------------------------------------------------
static lv_obj_t *scr_config;
static lv_obj_t *scr_connecting;
static lv_obj_t *scr_nav;
static lv_obj_t *scr_arrival;
static lv_obj_t *scr_clock;

// ---------------------------------------------------------------------------
// Nav screen  — icon + distance together + street name below
// ---------------------------------------------------------------------------
static lv_obj_t *img_maneuver;      // Image object for navigation arrow
static lv_obj_t *lbl_exit_number;   // Number inside roundabout icon
static lv_obj_t *lbl_distance_nav;  // Distance to next turn
static lv_obj_t *lbl_street;        // Street name (small)
static lv_obj_t *arc_progress;      // Arc progress along screen edge

// Arrival screen
static lv_obj_t *lbl_arrival_time;
static lv_obj_t *lbl_arrival_street;

// Clock screen
static lv_obj_t *lbl_clock;

// ---------------------------------------------------------------------------
// Helper: map maneuverId → LVGL image descriptor
// ---------------------------------------------------------------------------
static const lv_img_dsc_t* maneuverId_to_image(int id) {
  switch (id) {
    case 1: return &ui_img_ic_turn_left_png;          // Turn left
    case 2: return &ui_img_ic_turn_right_png;         // Turn right
    case 3: return &ui_img_ic_turn_slight_left_png;   // Bear / keep left
    case 4: return &ui_img_ic_turn_slight_right_png;  // Bear / keep right
    case 5: return &ui_img_ic_turn_u_turn_counterclockwise_png; // U-turn
    case 6: return &ui_img_ic_roundabout_clockwise_png;         // Roundabout
    case 7: return &ui_img_ic_destination_png;        // Arrive / destination
    default: return &ui_img_ic_straight_png;          // Straight ahead
  }
}

// ---------------------------------------------------------------------------
// Gesture callbacks  (cycle: Nav → Arrival → Clock → Nav)
// ---------------------------------------------------------------------------
static void btn_pair_evt(lv_event_t *e)    { ui_show_connecting(); }

static void gesture_nav_cb(lv_event_t *e) {
  lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
  if (dir == LV_DIR_LEFT || dir == LV_DIR_RIGHT)
    lv_scr_load_anim(scr_arrival, LV_SCR_LOAD_ANIM_FADE_ON, 250, 0, false);
}

static void gesture_arrival_cb(lv_event_t *e) {
  lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
  if (dir == LV_DIR_LEFT || dir == LV_DIR_RIGHT)
    lv_scr_load_anim(scr_clock, LV_SCR_LOAD_ANIM_FADE_ON, 250, 0, false);
}

static void gesture_clock_cb(lv_event_t *e) {
  lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
  if (dir == LV_DIR_LEFT || dir == LV_DIR_RIGHT)
    lv_scr_load_anim(scr_nav, LV_SCR_LOAD_ANIM_FADE_ON, 250, 0, false);
}

// ---------------------------------------------------------------------------
// ui_init
// ---------------------------------------------------------------------------
void ui_init() {

  // ── Config screen ──────────────────────────────────────────────────────────
  scr_config = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr_config, lv_color_black(), 0);

  lv_obj_t *title_cfg = lv_label_create(scr_config);
  lv_label_set_text(title_cfg, "Configuration");
  lv_obj_set_style_text_color(title_cfg, lv_color_white(), 0);
  lv_obj_set_style_text_font(title_cfg, &lv_font_montserrat_24, 0);
  lv_obj_align(title_cfg, LV_ALIGN_TOP_MID, 0, 30);

  lv_obj_t *btn_pair = lv_btn_create(scr_config);
  lv_obj_align(btn_pair, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(btn_pair, lv_color_make(50, 50, 50), 0);
  lv_obj_t *lbl_btn = lv_label_create(btn_pair);
  lv_label_set_text(lbl_btn, "Pair Phone");
  lv_obj_set_style_text_color(lbl_btn, lv_color_white(), 0);
  lv_obj_add_event_cb(btn_pair, btn_pair_evt, LV_EVENT_CLICKED, NULL);

  // ── Connecting screen ──────────────────────────────────────────────────────
  scr_connecting = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr_connecting, lv_color_black(), 0);
  lv_obj_t *lbl_conn = lv_label_create(scr_connecting);
  lv_label_set_text(lbl_conn, "Connecting...\nBluetooth LE");
  lv_obj_set_style_text_align(lbl_conn, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(lbl_conn, lv_color_white(), 0);
  lv_obj_set_style_text_font(lbl_conn, &lv_font_montserrat_24, 0);
  lv_obj_align(lbl_conn, LV_ALIGN_CENTER, 0, 0);

  // ── Navigation screen ──────────────────────────────────────────────────────
  scr_nav = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr_nav, lv_color_black(), 0);
  lv_obj_add_event_cb(scr_nav, gesture_nav_cb, LV_EVENT_GESTURE, NULL);
  lv_obj_clear_flag(scr_nav, LV_OBJ_FLAG_SCROLLABLE);

  // Arc progress along screen edge (270° sweep, matches 240×240 round display)
  arc_progress = lv_arc_create(scr_nav);
  lv_obj_set_size(arc_progress, 230, 230);
  lv_obj_center(arc_progress);
  lv_arc_set_rotation(arc_progress, 135);       // start at bottom-left
  lv_arc_set_bg_angles(arc_progress, 0, 270);   // 270° of arc
  lv_arc_set_range(arc_progress, 0, 100);
  lv_arc_set_value(arc_progress, 0);
  lv_obj_remove_style(arc_progress, NULL, LV_PART_KNOB); // no knob
  lv_obj_set_style_arc_width(arc_progress, 8, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc_progress, 8, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(arc_progress, lv_color_make(40, 40, 40), LV_PART_MAIN);
  lv_obj_set_style_arc_color(arc_progress, lv_color_white(), LV_PART_INDICATOR);
  lv_obj_clear_flag(arc_progress, LV_OBJ_FLAG_CLICKABLE);

  // Direction icon — downloaded PNG mapped to lv_img
  img_maneuver = lv_img_create(scr_nav);
  lv_img_set_src(img_maneuver, &ui_img_ic_straight_png);
  // Scale down the 192x192 icons by dividing zoom by 2. (256/2 = 128)
  lv_img_set_zoom(img_maneuver, 128);
  // The icons are black. Recolor them white so they contrast with the black background.
  lv_obj_set_style_img_recolor(img_maneuver, lv_color_white(), 0);
  lv_obj_set_style_img_recolor_opa(img_maneuver, LV_OPA_COVER, 0);
  lv_obj_align(img_maneuver, LV_ALIGN_CENTER, 0, -45);

  // Roundabout exit number (placed to the right of the image)
  lbl_exit_number = lv_label_create(scr_nav);
  lv_obj_set_style_text_font(lbl_exit_number, &lv_font_montserrat_32, 0);
  lv_label_set_text(lbl_exit_number, "");
  lv_obj_set_style_text_color(lbl_exit_number, lv_color_white(), 0);
  // Align it to the right of the center of the maneuver image (50px right)
  lv_obj_align_to(lbl_exit_number, img_maneuver, LV_ALIGN_CENTER, 50, 0);

  // Distance — directly below the icon
  lbl_distance_nav = lv_label_create(scr_nav);
  lv_obj_set_style_text_font(lbl_distance_nav, &lv_font_montserrat_32, 0);
  lv_label_set_text(lbl_distance_nav, "---");
  lv_obj_set_style_text_color(lbl_distance_nav, lv_color_make(200, 200, 200), 0);
  lv_obj_align(lbl_distance_nav, LV_ALIGN_CENTER, 0, 20);

  // Street name — small label at the bottom
  lbl_street = lv_label_create(scr_nav);
  lv_obj_set_width(lbl_street, 190);
  lv_label_set_long_mode(lbl_street, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_style_text_align(lbl_street, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(lbl_street, &lv_font_montserrat_24, 0);
  lv_label_set_text(lbl_street, "Waiting...");
  lv_obj_set_style_text_color(lbl_street, lv_color_make(140, 140, 140), 0);
  lv_obj_align(lbl_street, LV_ALIGN_CENTER, 0, 72);

  // ── Arrival screen ─────────────────────────────────────────────────────────
  scr_arrival = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr_arrival, lv_color_black(), 0);
  lv_obj_add_event_cb(scr_arrival, gesture_arrival_cb, LV_EVENT_GESTURE, NULL);
  lv_obj_clear_flag(scr_arrival, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *lbl_arr_title = lv_label_create(scr_arrival);
  lv_label_set_text(lbl_arr_title, "ARRIVAL");
  lv_obj_set_style_text_color(lbl_arr_title, lv_color_make(100, 100, 100), 0);
  lv_obj_set_style_text_font(lbl_arr_title, &lv_font_montserrat_24, 0);
  lv_obj_align(lbl_arr_title, LV_ALIGN_CENTER, 0, -58);

  lbl_arrival_time = lv_label_create(scr_arrival);
  lv_obj_set_style_text_font(lbl_arrival_time, &lv_font_montserrat_48, 0);
  lv_label_set_text(lbl_arrival_time, "--:--");
  lv_obj_set_style_text_color(lbl_arrival_time, lv_color_white(), 0);
  lv_obj_align(lbl_arrival_time, LV_ALIGN_CENTER, 0, 0);

  lbl_arrival_street = lv_label_create(scr_arrival);
  lv_obj_set_width(lbl_arrival_street, 180);
  lv_label_set_long_mode(lbl_arrival_street, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_style_text_align(lbl_arrival_street, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(lbl_arrival_street, &lv_font_montserrat_24, 0);
  lv_label_set_text(lbl_arrival_street, "---");
  lv_obj_set_style_text_color(lbl_arrival_street, lv_color_make(160, 160, 160), 0);
  lv_obj_align(lbl_arrival_street, LV_ALIGN_CENTER, 0, 60);

  // ── Clock screen ───────────────────────────────────────────────────────────
  scr_clock = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr_clock, lv_color_black(), 0);
  lv_obj_add_event_cb(scr_clock, gesture_clock_cb, LV_EVENT_GESTURE, NULL);
  lv_obj_clear_flag(scr_clock, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *lbl_clock_title = lv_label_create(scr_clock);
  lv_label_set_text(lbl_clock_title, "CLOCK");
  lv_obj_set_style_text_color(lbl_clock_title, lv_color_make(100, 100, 100), 0);
  lv_obj_set_style_text_font(lbl_clock_title, &lv_font_montserrat_24, 0);
  lv_obj_align(lbl_clock_title, LV_ALIGN_CENTER, 0, -50);

  lbl_clock = lv_label_create(scr_clock);
  lv_obj_set_style_text_font(lbl_clock, &lv_font_montserrat_48, 0);
  lv_label_set_text(lbl_clock, "00:00");
  lv_obj_set_style_text_color(lbl_clock, lv_color_white(), 0);
  lv_obj_align(lbl_clock, LV_ALIGN_CENTER, 0, 0);

  // Start on config screen
  lv_scr_load(scr_config);
}

// ---------------------------------------------------------------------------
// Screen helpers
// ---------------------------------------------------------------------------
void ui_show_config()     { lv_scr_load_anim(scr_config, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false); }
void ui_show_connecting() { lv_scr_load_anim(scr_connecting, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false); }
void ui_show_clock()      { lv_scr_load_anim(scr_clock, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false); }
void ui_show_arrival()    { lv_scr_load_anim(scr_arrival, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false); }
// ui_show_distance kept as a no-op stub so ble_manager.h does not need changes
void ui_show_distance()   { /* removed screen, falls through to nav */ }

// ---------------------------------------------------------------------------
// Main navigation update — updates all screen labels
// ---------------------------------------------------------------------------
void ui_show_navigation(int maneuverId, int exitNumber, const char *stepDistance, const char *eta,
                        const char *street, const char *currentTime,
                        int progressPercent) {

  // ── Clock ──────────────────────────────────────────────────────────────────
  if (currentTime && strlen(currentTime) > 0)
    lv_label_set_text(lbl_clock, currentTime);

  // ── Arrival screen ─────────────────────────────────────────────────────────
  if (eta && eta[0] != '\0') {
    lv_label_set_text(lbl_arrival_time, eta);
  } else {
    lv_label_set_text(lbl_arrival_time, currentTime ? currentTime : "--:--");
  }
  lv_label_set_text(lbl_arrival_street, street ? street : "");

  // ── Navigation screen ──────────────────────────────────────────────────────
  lv_img_set_src(img_maneuver, maneuverId_to_image(maneuverId));
  
  if (maneuverId == 6 && exitNumber > 0) { // Roundabout with specific exit
      lv_label_set_text_fmt(lbl_exit_number, "%d", exitNumber);
      // Move to foreground to ensure it draws on top of the image
      lv_obj_move_foreground(lbl_exit_number);
  } else {
      lv_label_set_text(lbl_exit_number, "");
  }

  lv_label_set_text(lbl_distance_nav, stepDistance ? stepDistance : "---");
  lv_label_set_text(lbl_street, street ? street : "");
  lv_arc_set_value(arc_progress, progressPercent);

  // Switch to nav unless the user is on one of the content screens
  lv_obj_t *active = lv_scr_act();
  if (active != scr_nav && active != scr_arrival && active != scr_clock) {
    lv_scr_load_anim(scr_nav, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false);
  }
}
