#include "ble_manager.h"
#include "ui.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <mutex>

// Thread-safe data exchange between NimBLE task and LVGL task
static std::mutex nav_mutex;
static int g_nav_maneuver_id = 0;
static String g_nav_instruction = "";
static String g_nav_street = "";
static String g_nav_distance = "";
static bool g_new_nav_data = false;

// UUIDs for the Custom Android Navigation Service (Matches Android
// BleClientManager)
#define SERVICE_UUID "12345678-1234-5678-1234-56789abcdef0"
#define CHARACTERISTIC_UUID "12345678-1234-5678-1234-56789abcdef1"

// ANCS Service IDs for iOS
#define ANCS_SERVICE_UUID "7905f431-b5ce-4e99-a40f-4b1e122d00d0"
#define ANCS_NOTIFICATION_SOURCE_UUID "9fbf120d-6301-42d9-8c58-25e699a21dbd"

static bool deviceConnected = false;
static NimBLEServer *pServer = nullptr;

class MyServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *pServer, ble_gap_conn_desc *desc) {
    deviceConnected = true;
    Serial.printf("BLE Client Connected! Handle: %d\n", desc->conn_handle);
    // Require security/pairing right away, crucial for both Android persistency
    // and iOS ANCS
    NimBLEDevice::startSecurity(desc->conn_handle);

    // Inform UI that we are connected, waiting for a route
    ui_show_navigation(0, "-", "Connected. Waiting for route...");
  }

  void onDisconnect(NimBLEServer *pServer, ble_gap_conn_desc *desc) {
    deviceConnected = false;
    Serial.println("BLE Client Disconnected!");
    ui_show_connecting();
    NimBLEDevice::startAdvertising();
  }
};

static int rx_count = 0;

// Callback for when Android App sends navigation details
class AndroidNavCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *pCharacteristic) {
    std::string rxValue = pCharacteristic->getValue();
    rx_count++;
    Serial.printf("BLE onWrite! Len: %d | Payload: %s\n", rxValue.length(),
                  rxValue.c_str());

    if (rxValue.length() > 0) {
      // Incoming format: maneuverId|instruction|street|distance
      String payload = String(rxValue.c_str());

      int firstPipe = payload.indexOf('|');
      int secondPipe = payload.indexOf('|', firstPipe + 1);
      int thirdPipe = payload.indexOf('|', secondPipe + 1);

      if (firstPipe != -1 && secondPipe != -1 && thirdPipe != -1) {
        int maneuverId = payload.substring(0, firstPipe).toInt();
        String instruction = firstPipe + 1 == secondPipe ? "" : payload.substring(firstPipe + 1, secondPipe);
        String street = secondPipe + 1 == thirdPipe ? "" : payload.substring(secondPipe + 1, thirdPipe);
        String distance = payload.substring(thirdPipe + 1);

        // Fallback ASCII Transliteration for Swedish Characters to avoid LVGL Squares
        // Since we don't have a 24px/32px custom font engine available on the host
        auto transliterate = [](String &str) {
            str.replace("å", "a");
            str.replace("ä", "a");
            str.replace("ö", "o");
            str.replace("Å", "A");
            str.replace("Ä", "A");
            str.replace("Ö", "O");
            // Also replace common multi-byte UTF-8 representations for åäö just in case
            str.replace("\xC3\xA5", "a"); // å
            str.replace("\xC3\xA4", "a"); // ä
            str.replace("\xC3\xB6", "o"); // ö
            str.replace("\xC3\x85", "A"); // Å
            str.replace("\xC3\x84", "A"); // Ä
            str.replace("\xC3\x96", "O"); // Ö
        };
        transliterate(instruction);
        transliterate(street);
        transliterate(distance);

        std::lock_guard<std::mutex> lock(nav_mutex);
        g_nav_maneuver_id = maneuverId;
        g_nav_instruction = instruction;
        g_nav_street = street;
        g_nav_distance = distance;
        g_new_nav_data = true;
      } else {
        // Fallback if parsing fails
        std::lock_guard<std::mutex> lock(nav_mutex);
        g_nav_maneuver_id = 0;
        g_nav_instruction = "Nav";
        g_nav_street = "-";
        g_nav_distance = rxValue.c_str();
        g_new_nav_data = true;
      }
    }
  }
};

void ble_process_nav_data() {
  std::lock_guard<std::mutex> lock(nav_mutex);
  if (g_new_nav_data) {
    g_new_nav_data = false;
    ui_show_navigation(g_nav_maneuver_id, g_nav_distance.c_str(),
                       g_nav_street.c_str());
  }
}

void ble_init() {
  NimBLEDevice::init("ESP32_Nav");

  // Set security for pairing (required for ANCS and bonding)
  NimBLEDevice::setSecurityAuth(true, true, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
  NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC |
                                   BLE_SM_PAIR_KEY_DIST_ID);
  NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC |
                                   BLE_SM_PAIR_KEY_DIST_ID);

  // Create the BLE Server
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // 1. Setup Custom Service for Android App
  NimBLEService *pNavService = pServer->createService(SERVICE_UUID);
  NimBLECharacteristic *pNavChar = pNavService->createCharacteristic(
      CHARACTERISTIC_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE |
                               NIMBLE_PROPERTY::WRITE_NR |
                               NIMBLE_PROPERTY::NOTIFY);
  pNavChar->setCallbacks(new AndroidNavCallbacks());
  pNavService->start();

  // Start advertising
  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  // Also advertise service data requesting iOS to bond if we wanted
  pAdvertising->setScanResponse(true);
  pAdvertising->start();
}

bool ble_is_connected() { return deviceConnected; }
