#pragma once
#include <Arduino.h>

void ble_init();
bool ble_is_connected();
void ble_process_nav_data();
