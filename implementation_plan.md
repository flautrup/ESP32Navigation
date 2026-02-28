# ESP32 Motorcycle Navigation System - Implementation Plan

## Goal Description
The goal is to build a turn-by-turn navigation display for a motorcycle using an ESP32, a GC9A01 round display, and a CST816S touch controller. The system will receive navigation instructions from Google Maps running on a connected smartphone (Android or iOS) via Bluetooth or WiFi.

## User Review Required
> [!WARNING]
> **iOS Limitation:** iOS does not allow companion background apps to read notifications from other apps (like Google Maps). The standard workaround for iOS is to have the ESP32 act as an Apple Notification Center Service (ANCS) client. This means the ESP32 connects directly to the iPhone via BLE, reads *all* iOS notifications natively, filters for "com.google.Maps", and parses the notification text for instructions. Android, on the other hand, can use a dedicated companion app with `NotificationListenerService`.
> Are you comfortable with this ANCS approach for iOS?

> [!NOTE]
> **Bluetooth vs WiFi:** Bluetooth Low Energy (BLE) is strongly recommended over WiFi. BLE is much better for background connections on phones, uses significantly less power, and doesn't require the user to enable a mobile hotspot on their phone. This plan assumes BLE for communication.

## Proposed System Architecture

### 1. ESP32 Firmware Development
The core of the system will be written in C/C++ (likely using ESP-IDF or Arduino core, depending on your preference).
- **UI Framework:** LVGL (Light and Versatile Graphics Library) to create a beautiful, monochrome-themed UI with anti-aliasing, similar to your previous Timekeeper project. The UI will be specifically designed for the 240x240 round display.
- **Display Driver:** GC9A01 SPI driver integration with LVGL for the 240x240 round IPS screen.
- **Touch Driver:** CST816S I2C driver integration to interpret swipe/tap gestures (e.g., to dismiss instructions, change brightness, or sleep the display).
- **BLE Stack:** 
  - *Android Mode:* A custom BLE UUID service to receive pre-parsed navigation data (Distance, Maneuver Icon/Type, Street Name) sent by the Android companion app.
  - *iOS Mode:* An ANCS client to subscribe to iOS notifications natively, parsing the Google Maps notification text on the ESP32.
- **State Machine:** 
  - `Screen 0: Configuration` -> Use touch to manage settings, pair phones, or set theme/brightness.
  - `Screen 1: Connecting` -> `Screen 2: Waiting for Route` -> `Screen 3: Active Navigation` (showing turn arrow, distance remaining, street).

### 2. Android Companion App
A dedicated Android application will be required to broker the data.
- **Background Service:** Implements Android's `NotificationListenerService` to capture Google Maps navigation notifications in the background.
- **Data Parser:** Extracts the maneuver icon (left, right, straight, roundabout), distance to turn, and street name from the notification extras.
- **BLE Client:** Connects to the ESP32's BLE service and transmits the extracted short-form data as a JSON or binary payload (e.g., `{"dir":"left", "dist":"300m", "str":"Main St"}`).

### 3. iOS Companion (Optional / Contextual)
Since the ESP32 will use ANCS to read notifications natively, an iOS app is technically not required for core functionality, though you might want a simple app to configure the ESP32 (e.g. WiFi credentials if needed, or theme settings).

## Verification Plan
- **Phase 1: Hardware & UI Setup.** Flash ESP32 with GC9A01 and LVGL. Create mock navigation screens (hardcoded arrows and distances) and the configuration UI to verify visual aesthetics on the physical 240x240 round display.
- **Phase 2: BLE Communication (ESP32).** Write a mock BLE sender (using a Python script or mobile BLE tool like nRF Connect) to send JSON payloads to the ESP32 and see the UI update in real-time.
- **Phase 3: Android App & End-to-End.** Deploy the Android app, start Google Maps navigation to a random location, and verify the ESP32 screen updates correctly as the phone navigates.
- **Phase 4: iOS ANCS Parsing.** Pair an iPhone to the ESP32 via BLE. Trigger simulated Google Maps notifications (start a route) and verify the ESP32 correctly filters and extracts the route text/icons.
- **Phase 6: UI Enhancements.** 
  - **Android Parser:** Update `NavNotificationService` to regex/match the Google Maps instruction string (e.g., "turn right", "roundabout") and emit a single integer `Maneuver ID` (0=Straight, 1=Right, 2=Left, etc.).
  - **BLE Transmission:** Prepend the Maneuver ID to the CSV payload (`"1,Turn right,Main St,300m"`).
  - **ESP32 Rendering:** Update `ui.cpp` to map the Maneuver ID to built-in LVGL vector icons (e.g., `LV_SYMBOL_RIGHT`, `LV_SYMBOL_UP`). Apply high-contrast monochrome styles and scale up LVGL fonts (e.g., `lv_font_montserrat_28` or larger bounds) to ensure maximum readability while riding.
