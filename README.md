# ESP32 Motorcycle Navigation System

This project transforms an ESP32-C3 microcontroller with a 240x240 round display (GC9A01) into a live motorcycle navigation client. It integrates seamlessly with Google Maps running on an Android companion smartphone via Bluetooth Low Energy (BLE), offering a highly power-efficient and seamless turn-by-turn navigation experience for riders.

## Features
- **Live Turn-By-Turn Navigation**: Real-time instructions (e.g., "Turn right", "300m", "Main Street") displayed on the round dashboard screen.
- **Progress Bar**: Relative journey progress indicator based on ETA and current time.
- **Clock Screen**: Dedicated swipeable digital clock synced from your smartphone.
- **Cross-Platform Support**:
  - **Android**: Custom companion app for structured data transmission.
  - **iOS (iPhone)**: Native support via Apple Notification Center Service (ANCS) – No app required!
- **Power-Efficient BLE**: Transmits navigation instructions over BLE, saving battery compared to WiFi/Hotspot.
- **Swedish Support**: Automatic transliteration of Swedish characters (å, ä, ö) for maximum readability.

## Hardware Requirements
- **ESP32-C3** based Development Board (e.g. ESP32-2424S012C).
- **GC9A01** SPI Display (240x240 pixels).
- **CST816S** I2C Touch Controller.
- Android Smartphone (Android 11+) OR iPhone (iOS 15+).

## Installation and Setup

### 1. ESP32 Firmware Installation
The firmware is built using PlatformIO.
1. Open the `ESP32` folder in VS Code with the PlatformIO extension installed.
2. Connect your ESP32-C3 board via USB and run **Upload**.

### 2. Smartphone Connection

#### **Using Android**
1. Build and install the `AndroidApp` (see `AndroidApp/README.md` for gradle details).
2. Open the app and grant **Bluetooth** and **Notification Access** permissions.
3. Tap **Connect**. The ESP32 will show "Connected. Waiting for route...".
4. Start navigation in **Google Maps**.

#### **Using iPhone (iOS)**
1. Power on the ESP32.
2. Open **Settings > Bluetooth** on your iPhone.
3. Find and select **ESP32_Nav**.
4. Confirm the **Pairing Request** on your iPhone.
5. When prompted, allow the device to **Show Notifications**. 
   - *Note: Ensure "Show Previews" is set to "Always" or "When Unlocked" in Settings > Notifications for your Maps app.*
6. Start a route in **Google Maps** or **Apple Maps**.

### 3. Using the Display
- **Navigation Screen**: Shows travel icons, instructions, street names, and the **Progress Bar** at the bottom.
- **Switch to Clock**: **Swipe Left or Right** on the display to toggle between the navigation view and a large digital clock.

## Development Libraries
- **PlatformIO / Arduino Core**
- **LovyanGFX** (Display drivers)
- **LVGL** (GUI library)
- **NimBLE-Arduino** (BLE stack)

