# ESP32 Motorcycle Navigation - Project Walkthrough

## Project Overview
This project successfully transformed an ESP32-C3 with a GC9A01 240x240 round display into a live motorcycle navigation client that integrates directly with Google Maps via a companion Android application. 

By bypassing the need for a persistent internet connection (WiFi routing) and instead relying exclusively on **Bluetooth Low Energy (BLE)**, the system achieves a highly power-efficient and seamless background connection to the rider's phone.

## Architecture & Implementation Highlights

### 1. The Android Intercept Engine (`NavNotificationService`)
Traditional navigation apps don't expose live routing APIs to external Bluetooth devices. To solve this, we implemented an Android `NotificationListenerService`.
When the user locks their phone with Google Maps running, Google Maps pushes detailed turn-by-turn vectors to the Android Notification Shade. Our service silently intercepts these notifications in the background, parses the bundle metadata, and extracts:
- `Instruction` (e.g., "Turn right")
- `Street` (e.g., "Mannerheimintie")
- `Distance` (e.g., "300m")

### 2. The Custom BLE Pipeline (`BleClientManager`)
To transmit this payload without draining battery, we constructed a dedicated BLE GATT client with custom UUIDs:
- **Service UUID:** `12345678-1234-5678-1234-56789abcdef0`
- **Char UUID:** `12345678-1234-5678-1234-56789abcdef1`

**Key Engineering Solutions:**
*   **The 512-Byte MTU Dilemma:** Standard BLE is locked to a 20-byte Maximum Transmission Unit (MTU), which instantly truncated long street names. We implemented a dynamic MTU negotiation sequence in Android to expand the pipe to 512 bytes.
*   **The Android Discovery Trap:** Android 11+ frequently hangs if an MTU expansion is requested immediately after connection. We engineered a specific event hierarchy: Connect -> Discover Services -> Negotiate MTU -> Transmit.
*   **GATT Cache Poisoning:** During development, the ESP32 crashing would permanently cache "empty" service tables in the Android phone's memory. We injected a targeted Reflection hack (`java.lang.reflect.Method`) to forcefully overwrite the Android internal cache on every connection.
*   **Fire-and-Forget (WRITE_NO_RSP):** Standard BLE `WRITE` locks the Android radio until the ESP32 acknowledges the packet, leading to unhandled queues and lag. We upgraded the characteristic to `WRITE_NO_RSP` (Write without Response) to enable sub-millisecond, non-blocking telemetry streams.

### 3. Deep Embedded Hardware Solutions (ESP32 / C++)
The ESP32 firmware manages dual-core routing, intercepting the BLE payload while painting the LVGL round UI.

*   **The `spiAttachMISO` Conflict:** The `ESP32-2424S012C` dev board does not wire a Master-In-Slave-Out (MISO) pin for its screen since it doesn't need to read pixel data back. However, when the NimBLE stack boots, the ESP32-HAL enforces a strict hardware bus check that caused immediate Core Panics since MISO was tied to `-1`. We resolved this by assigning MISO to a dummy floating GPIO pin (`Pin 8`).
*   **The `LVGL Mutex` Thread-Safety Issue:** Initially, whenever the Android phone transmitted a string ("Turn right"), the ESP32 crashed instantly. This occurred because NimBLE runs inside a background RTOS hardware interrupt (`onWrite`), but LVGL requires all UI updates to occur exclusively inside the main `loop()` thread.
    We solved this by implementing a `std::mutex` data parking lot:
    ```cpp
    std::lock_guard<std::mutex> lock(nav_mutex);
    g_nav_street = street;
    g_new_nav_data = true;
    ```
    The fast-interrupt radio drops the parcel here and leaves, while the slow `loop()` safely picks up the parcel and paints `ui_show_navigation` moments later.

## Verification
- **End-to-End Success:** Live Android Notification testing verified the exact phrase *"Starting navigation"* was successfully parsed out of Google Maps data, squashed into the custom MTU payload, transmitted over NimBLE, parked under a Mutex, and painted by `lv_timer_handler()` onto the round dashboard screen.

## Next Steps
Now that the backbone pipeline works perfectly, you can customize the UI formatting to parse the instructions (perhaps linking specific instruction substrings to draw actual graphical arrows in LVGL) or design the Configuration menus for theme brightness.
