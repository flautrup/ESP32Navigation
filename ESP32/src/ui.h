#pragma once
#include <lvgl.h>

// Initialize the UI (creates base objects)
void ui_init();

// Show the Configuration/Settings screen
void ui_show_config();

// Show the "Connecting to phone" screen
void ui_show_connecting();

// Show the active navigation screen with turn details
void ui_show_navigation(const char *maneuver, const char *distance,
                        const char *street);
