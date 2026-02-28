# ESP32 Motorcycle Navigation System

This project transforms an ESP32-C3 microcontroller with a 240x240 round display (GC9A01) into a live motorcycle navigation client. It integrates seamlessly with Google Maps running on an Android companion smartphone via Bluetooth Low Energy (BLE), offering a highly power-efficient and seamless turn-by-turn navigation experience for riders.

## Features
- **Live Turn-By-Turn Navigation**: Real-time instructions (e.g., "Turn right", "300m", "Main Street") displayed beautifully on the round dashboard screen.
- **Power-Efficient BLE Transfer**: The Android companion app transmits navigation instructions over BLE (`WRITE_NO_RSP`), avoiding the heavy battery drain of WiFi hotspots.
- **Custom Android Notification Intercept**: Captures and parses detailed Google Maps notification vectors seamlessly in the background using Android's `NotificationListenerService`.
- **Thread-safe Render Engine (LVGL)**: Uses a `std::mutex` parser that securely parses high-speed radio data into a safe memory space so the LVGL Graphics Engine can render it smoothly on the main thread without crashing.

## Hardware Requirements
- **ESP32-C3** based Development Board (e.g. ESP32-2424S012C).
- **GC9A01** SPI Display (240x240 pixels).
- **CST816S** I2C Touch Controller.
- Android Smartphone (Android 11+ recommended).

## Installation and Setup

The repository is structured into two completely independent parts: the `ESP32` firmware and the `AndroidApp` codebase.

### 1. ESP32 Firmware Installation
The firmware is built using PlatformIO.

1. Install **Visual Studio Code** and the **PlatformIO** extension.
2. Open the `ESP32` folder in VS Code.
3. Connect your ESP32-C3 board via USB.
4. Run the PlatformIO "Upload" task (or run `pio run -t upload` in the terminal).
*(Note: Initial setup uses a custom `LovyanGFX` and `lvgl` driver configuration mapped for the `ESP32-2424S012C` dev board. If you are using a slightly different board, you may need to adjust the SPI/I2C pin alignments in `main.cpp`).*

### 2. Android Companion App Installation
The Android companion app functions as a middleman, securely reading what Google Maps prints to your notification bar and beaming the data to the ESP32 via BLE.

**Building via Android Studio:**
1. Open the `AndroidApp` directory in Android Studio.
2. Wait for Gradle to perform its background sync.
3. Build the APK and install it to your smartphone device. 

**Building via Gradle CLI (No Android Studio):**
1. Ensure Java (JDK 17+) and the Android SDK are configured in your path (`ANDROID_HOME`).
2. Navigate to `AndroidApp` and run: `./gradlew assembleDebug`
3. The APK will compile into `app/build/outputs/apk/debug/app-debug.apk`. 
4. Transfer this APK to your phone (via USB, Google Drive, etc) and install it.

### 3. Execution & Testing
1. On your phone, open the **ESP32 Navigation** companion App.
2. Grant the required permissions when prompted (Bluetooth, Nearby Devices, Notification Access).
3. Ensure the ESP32 is powered on. Tap **Connect** in the Android app.
   - *Android occasionally aggressively caches broken Bluetooth device memories. The app uses a reflection reset mechanism, but if it fails to bind, restart the Android Bluetooth Radio.*
4. The ESP32 screen will show `Connected. Waiting for route...`.
5. Background the ESP32 Companion App and open **Google Maps**.
6. Select a destination that requires driving and hit **Start Navigation**.
7. Lock your phone to force Google Maps to post the deep-route navigation notification vectors to the shade.
8. The ESP32 screen will automatically catch the updates and draw the route!

## Development Libraries Used
- **PlatformIO / Arduino Core** (Firmware engine)
- **LovyanGFX** (High performance DMA display drivers)
- **LVGL** (Embedded GUI library)
- **NimBLE-Arduino** (Extremely lightweight BLE Bluetooth stack)
