#pragma once
#include <lvgl.h>

// Initialize the UI (creates base objects)
void ui_init();

// Show the Configuration/Settings screen
void ui_show_config();

// Show the "Connecting to phone" screen
void ui_show_connecting();

// Show the active navigation screen with turn details
void ui_show_navigation(int maneuverId, const char *distance,
                        const char *street, const char *currentTime,
                        int progressPercent);

// Show the clock screen
void ui_show_clock();

// Show the arrival time + street screen
void ui_show_arrival();

// Show the large distance/direction-only screen
void ui_show_distance();
