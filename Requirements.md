# ESP32 Navigation

## Goal
Create a navigation system for a motorcycles. 
Use an ESP32 for visualizing turn by turn navigation on the motorcycle connected to your phone. The Phone should use Google Maps for navigation and send the turn by turn navigation to the ESP32. The ESP32 should display the turn by turn navigation on the GC9A01 round 240x240 IPS display.

Connection to phone should be by Bluetooth Low Energy (BLE). No WiFi will be used.

It should be possible to install apps on the phones without the need to use app stores.

## User flow [DONE]
1. User turns on the motorcycle
2. User turns on the ESP32 navigation system
3. ESP32 navigation system connects to the phone (via Android App or native iOS Bluetooth)
4. User opens Google Maps (or Apple Maps on iOS)
5. User enters destination
6. Instructions are sent to ESP32 via BLE
7. ESP32 displays turn details, street name, and progress
8. Navigation ends, user turns off bike

## Implemented Features
- **[DONE] BLE Connectivity**: Robust Low Energy communication for long battery life.
- **[DONE] Cross-Platform**: 
    - **Android**: Custom companion app with notification listening.
    - **iOS**: Native support via **ANCS** (no app required).
- **[DONE] UI Screens**:
    - **Navigation View**: Visual icons, street names, and distance.
    - **Progress Bar**: Relative journey progress indicator.
    - **Clock Screen**: Full-screen digital clock accessible via swipe.
- **[DONE] Localization**: Automatic transliteration of Swedish characters (å, ä, ö) for the display.
- **[DONE] Touch Control**: Swipe gestures (Left/Right) to switch between Navigation and Clock.

## Hardware [DONE]
- ESP32-C3
- GC9A01 round 240x240 IPS display
- CST816S touch controller
- Android & iOS Smartphones


