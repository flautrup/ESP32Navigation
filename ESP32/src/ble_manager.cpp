#include "ble_manager.h"
#include "ui.h"
#include <NimBLEDevice.h>

// UUIDs for the Custom Android Navigation Service
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// ANCS Service IDs for iOS
#define ANCS_SERVICE_UUID "7905f431-b5ce-4e99-a40f-4b1e122d00d0"
#define ANCS_NOTIFICATION_SOURCE_UUID "9fbf120d-6301-42d9-8c58-25e699a21dbd"

static bool deviceConnected = false;
static NimBLEServer *pServer = nullptr;

class MyServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *pServer, ble_gap_conn_desc *desc) {
    deviceConnected = true;
    // Require security/pairing right away, crucial for both Android persistency
    // and iOS ANCS
    NimBLEDevice::startSecurity(desc->conn_handle);

    // Inform UI that we are connected, waiting for a route
    ui_show_navigation("-", "-", "Connected. Waiting for route...");
  }

  void onDisconnect(NimBLEServer *pServer, ble_gap_conn_desc *desc) {
    deviceConnected = false;
    ui_show_connecting();
    NimBLEDevice::startAdvertising();
  }
};

// Callback for when Android App sends navigation details
class AndroidNavCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *pCharacteristic) {
    std::string rxValue = pCharacteristic->getValue();
    if (rxValue.length() > 0) {
      // For now, naive display mapping.
      // In a real scenario we'd parse JSON or pipe-delimited values like
      // "Left|300m|Main St"
      ui_show_navigation("->", "N/A", rxValue.c_str());
    }
  }
};

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
      CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
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
