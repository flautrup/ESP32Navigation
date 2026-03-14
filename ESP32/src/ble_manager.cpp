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
static String g_nav_current_time = "00:00";
static int g_nav_progress_percent = 0;
static int g_max_trip_minutes = -1;
static bool g_new_nav_data = false;

// Helper to extract minutes from "HH:MM"
static int parseTimeToMinutes(const String &timeStr) {
  int colonIdx = timeStr.indexOf(':');
  if (colonIdx > 0 && colonIdx < timeStr.length() - 1) {
    int h = timeStr.substring(0, colonIdx).toInt();
    int m = timeStr.substring(colonIdx + 1).toInt();
    return h * 60 + m;
  }
  return -1;
}

// Fallback ASCII Transliteration for Swedish Characters to avoid LVGL Squares
static void transliterate(String &str) {
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
}

// Ported from Android NavNotificationService to allow ESP32 to deduce icons from raw iOS text
static int parseManeuverId(String instruction) {
  if (instruction.length() == 0) return 0;
  String lower = instruction;
  lower.toLowerCase();
  lower.trim();

  if (lower.indexOf("u-turn") != -1) return 5;
  if (lower.indexOf("roundabout") != -1) return 6;
  if (lower.indexOf("keep left") != -1 || lower.indexOf("bear left") != -1) return 3;
  if (lower.indexOf("keep right") != -1 || lower.indexOf("bear right") != -1) return 4;
  if (lower.indexOf("left") != -1) return 1;
  if (lower.indexOf("right") != -1) return 2;
  if (lower.indexOf("arrive") != -1 || lower.indexOf("destination") != -1) return 7;

  return 0; // Default straight arrow
}

void ble_process_nav_data() {
  std::lock_guard<std::mutex> lock(nav_mutex);
  if (g_new_nav_data) {
    g_new_nav_data = false;
    ui_show_navigation(g_nav_maneuver_id, g_nav_distance.c_str(),
                       g_nav_street.c_str(), g_nav_current_time.c_str(),
                       g_nav_progress_percent);
  }
}

// UUIDs for the Custom Android Navigation Service (Matches Android
// BleClientManager)
#define SERVICE_UUID "12345678-1234-5678-1234-56789abcdef0"
#define CHARACTERISTIC_UUID "12345678-1234-5678-1234-56789abcdef1"

// ANCS Service IDs for iOS
#define ANCS_SERVICE_UUID "7905f431-b5ce-4e99-a40f-4b1e122d00d0"
#define ANCS_NOTIFICATION_SOURCE_UUID "9fbf120d-6301-42d9-8c58-25e699a21dbd"
#define ANCS_CONTROL_POINT_UUID "69d1d94e-d5ad-49a4-9117-21bc8670873b"
#define ANCS_DATA_SOURCE_UUID "22e91511-4454-465a-a555-5e1703b503c4"

static bool deviceConnected = false;
static NimBLEServer *pServer = nullptr;
static NimBLEClient *pClient = nullptr;

// Flags and storage for ANCS
static uint32_t active_notification_uid = 0;
static String ancs_title = "";
static String ancs_message = "";

// ANCS Callbacks
void ancs_notification_callback(NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify);
void ancs_data_callback(NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify);

class MyServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *pServer, ble_gap_conn_desc *desc) {
    deviceConnected = true;
    Serial.printf("BLE Client Connected! Handle: %d\n", desc->conn_handle);
    // Require security/pairing right away, crucial for both Android persistency
    // and iOS ANCS
    NimBLEDevice::startSecurity(desc->conn_handle);

    // Inform UI that we are connected, waiting for a route
    ui_show_navigation(0, "-", "Connected. Waiting for route...", "00:00", 0);

    // Start discovery of ANCS on the connected phone
    if (pClient == nullptr) pClient = NimBLEDevice::createClient();
    if (pClient->connect(NimBLEAddress(desc->peer_ota_addr))) {
        Serial.println("Client connected to iPhone. Looking for ANCS...");
        NimBLERemoteService* pAncsService = pClient->getService(ANCS_SERVICE_UUID);
        if (pAncsService) {
            Serial.println("ANCS Service Found!");
            NimBLERemoteCharacteristic* pNotifSource = pAncsService->getCharacteristic(ANCS_NOTIFICATION_SOURCE_UUID);
            if (pNotifSource && pNotifSource->canNotify()) {
                pNotifSource->subscribe(true, ancs_notification_callback);
            }
            NimBLERemoteCharacteristic* pDataSource = pAncsService->getCharacteristic(ANCS_DATA_SOURCE_UUID);
            if (pDataSource && pDataSource->canNotify()) {
                pDataSource->subscribe(true, ancs_data_callback);
            }
        }
    }
  }

    void onDisconnect(NimBLEServer *pServer, ble_gap_conn_desc *desc) {
    deviceConnected = false;
    Serial.println("BLE Client Disconnected!");
    if (pClient) {
        pClient->disconnect();
        // We don't necessarily need to delete the client, just disconnect
    }
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
      // Incoming format: maneuverId|instruction|street|distance|currentTime
      String payload = String(rxValue.c_str());

      int firstPipe = payload.indexOf('|');
      int secondPipe = payload.indexOf('|', firstPipe + 1);
      int thirdPipe = payload.indexOf('|', secondPipe + 1);
      int fourthPipe = payload.indexOf('|', thirdPipe + 1);

      if (firstPipe != -1 && secondPipe != -1 && thirdPipe != -1 && fourthPipe != -1) {
        int maneuverId = payload.substring(0, firstPipe).toInt();
        String instruction = firstPipe + 1 == secondPipe ? "" : payload.substring(firstPipe + 1, secondPipe);
        String street = secondPipe + 1 == thirdPipe ? "" : payload.substring(secondPipe + 1, thirdPipe);
        String distance = thirdPipe + 1 == fourthPipe ? "" : payload.substring(thirdPipe + 1, fourthPipe);
        String currentTime = payload.substring(fourthPipe + 1);

        transliterate(instruction);
        transliterate(street);
        transliterate(distance);

        // Calculate Route Progress if distance contains a time (e.g. "Ankomst 13:32")
        int progress = 0;
        int etaColon = distance.indexOf(':');
        if (etaColon >= 2) {
          // Extract HH:MM assuming two chars before the colon. 
          // If distance is "Ankomst 13:32", this extracts "13:32"
          String etaStr = distance.substring(etaColon - 2, etaColon + 3);
          int etaMins = parseTimeToMinutes(etaStr);
          int curMins = parseTimeToMinutes(currentTime);

          if (etaMins != -1 && curMins != -1) {
            // Handle midnight rollover
            if (etaMins < curMins && (curMins - etaMins) > 12 * 60) {
              etaMins += 24 * 60;
            }
            int remaining = etaMins - curMins;
            if (remaining < 0) remaining = 0;

            // Reset max trip minutes if the trip got longer (e.g. new destination)
            if (g_max_trip_minutes < remaining) {
              g_max_trip_minutes = remaining;
            }

            if (g_max_trip_minutes > 0) {
              progress = 100 - ((remaining * 100) / g_max_trip_minutes);
            }
          }
        } else {
            // No ETA available, reset trip tracking so next nav isn't broken
            g_max_trip_minutes = -1;
        }

        if (progress < 0) progress = 0;
        if (progress > 100) progress = 100;

        std::lock_guard<std::mutex> lock(nav_mutex);
        g_nav_maneuver_id = maneuverId;
        g_nav_instruction = instruction;
        g_nav_street = street;
        g_nav_distance = distance;
        g_nav_current_time = currentTime;
        g_nav_progress_percent = progress;
        g_new_nav_data = true;
      } else {
        // Fallback if parsing fails
        std::lock_guard<std::mutex> lock(nav_mutex);
        g_nav_maneuver_id = 0;
        g_nav_instruction = "Nav";
        g_nav_street = "-";
        g_nav_distance = rxValue.c_str();
        g_nav_current_time = "RTE";
        g_nav_progress_percent = 0;
        g_new_nav_data = true;
      }
    }
  }
};

// Removed and placed at the top of the file

void ancs_notification_callback(NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
    if (length < 8) return;
    uint8_t eventId = pData[0];
    uint32_t uid = pData[4] | (pData[5] << 8) | (pData[6] << 16) | (pData[7] << 24);

    if (eventId == 0) { // EventIDNotificationAdded
        active_notification_uid = uid;
        Serial.printf("ANCS: New Notification. UID: %u. Requesting attributes...\n", uid);

        // Request Title (ID 1) and Message (ID 3)
        uint8_t getAttributes[] = {
            0x00, // CommandIDGetNotificationAttributes
            (uint8_t)(uid & 0xFF), (uint8_t)((uid >> 8) & 0xFF), (uint8_t)((uid >> 16) & 0xFF), (uint8_t)((uid >> 24) & 0xFF),
            1, // ID Title
            0x00, 0x10, // Max length 256 (little endian?) - actually 0x00 0x40 is 64
            3, // ID Message
            0x00, 0x40  // Max length 64
        };
        
        NimBLERemoteService* pAncsService = pChar->getRemoteService();
        if (pAncsService) {
            NimBLERemoteCharacteristic* pCP = pAncsService->getCharacteristic(ANCS_CONTROL_POINT_UUID);
            if (pCP) pCP->writeValue(getAttributes, sizeof(getAttributes), false);
        }
    }
}

void ancs_data_callback(NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
    // Basic parser for ANCS Data Source responses
    // Format: CommandID(1) | UID(4) | AttributeID(1) | Length(2) | Data(var)
    if (length < 8) return;
    
    uint8_t attrId = pData[5];
    uint16_t attrLen = pData[6] | (pData[7] << 8);
    String content = "";
    for(int i=0; i<attrLen && (i+8)<length; i++) {
        content += (char)pData[i+8];
    }

    if (attrId == 1) ancs_title = content;
    if (attrId == 3) ancs_message = content;

    // If we have both, or if we just got a message, try to update UI
    if (ancs_title.length() > 0 || ancs_message.length() > 0) {
        Serial.printf("ANCS Data: [%s] %s\n", ancs_title.c_str(), ancs_message.c_str());
        
        // Basic filter: Only process if it looks like navigation
        // In a real scenario, we'd check the AppID, but for now we look for keywords
        String combined = ancs_title + " " + ancs_message;
        String combinedLower = combined;
        combinedLower.toLowerCase();

        if (combinedLower.indexOf("map") != -1 || combinedLower.indexOf("svang") != -1 || combinedLower.indexOf("straight") != -1) {
            transliterate(ancs_title);
            transliterate(ancs_message);
            std::lock_guard<std::mutex> lock(nav_mutex);
            g_nav_instruction = ancs_title;
            g_nav_street = ancs_message;
            g_nav_maneuver_id = parseManeuverId(ancs_title);
            g_nav_distance = ""; // iOS notifications don't typically separate distance cleanly
            g_new_nav_data = true;
        }
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
