# ESP32 Navigation

## Goal
Create a navigation system for a motorcycles. 
Use an ESP32 for visualizing turn by turn navigation on the motorcycle connected to your phone. The Phone should use Google Maps for navigation and send the turn by turn navigation to the ESP32. The ESP32 should display the turn by turn navigation on the GC9A01 round 240x240 IPS display.

Connection to phone should be by Bluetooth Low Energy (BLE). No WiFi will be used.

It should be possible to install apps on the phones without the need to use app stores.

## User flow
1. User turns on the motorcycle
2. User turns on the ESP32 navigation system
3. ESP32 navigation system connects to the phone
4. User opens Google Maps on the phone
5. User enters destination in Google Maps
6. Google Maps sends turn by turn navigation to the ESP32 (via an Android companion app or iOS ANCS)
7. ESP32 displays turn by turn navigation on the GC9A01 round 240x240 IPS display
8. User turns off the motorcycle
9. ESP32 navigation system turns off

## Optional features
- Able to use sound from phone connected to bluetooth headphones
- Able to use touch controller to control the ESP32 navigation system

## Additional Requirements
- **BLE LE**: Bluetooth Low Energy (LE) MUST be used for all phone-to-ESP32 communication.
- **iOS Strategy**: iOS MUST use Apple Notification Center Service (ANCS) with the ESP32 acting as an ANCS Client to read notifications directly without a custom iOS app.
- **Configuration UI**: The ESP32 MUST have an on-device configuration UI to manage settings directly.

## Hardware
- ESP32
- GC9A01 round 240x240 IPS display
- CST816S touch controller
- Google Pixel 9 with Android
- iPhone 15 with iOS


