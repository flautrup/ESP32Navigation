#include "ble_manager.h"
#include "ui.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <mutex>

// Thread-safe data exchange between NimBLE task and LVGL task
static std::mutex nav_mutex;
static int g_nav_maneuver_id = 0;
static int g_nav_exit_number = 0;
static String g_nav_instruction = ""; // Title
static String g_nav_street = "";      // Street
static String g_nav_step_distance = ""; // Distance to next turn
static String g_nav_eta = "";         // Arrival time
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

static int parseManeuverId(String instruction) {
  if (instruction.length() == 0) return 0;
  String lower = instruction;
  lower.toLowerCase();
  lower.trim();

  // English
  if (lower.indexOf("u-turn") != -1) return 5;
  if (lower.indexOf("roundabout") != -1) return 6;
  if (lower.indexOf("keep left") != -1 || lower.indexOf("bear left") != -1) return 3;
  if (lower.indexOf("keep right") != -1 || lower.indexOf("bear right") != -1) return 4;
  if (lower.indexOf("left") != -1) return 1;
  if (lower.indexOf("right") != -1) return 2;
  if (lower.indexOf("arrive") != -1 || lower.indexOf("destination") != -1) return 7;

  // Swedish (transliterated)
  if (lower.indexOf("u-svang") != -1 || lower.indexOf("vand om") != -1) return 5;
  if (lower.indexOf("rondell") != -1 || lower.indexOf("cirkulationsplats") != -1) return 6;
  if (lower.indexOf("hall vanster") != -1 || lower.indexOf("till vanster") != -1) return 3;
  if (lower.indexOf("hall hoger") != -1 || lower.indexOf("till hoger") != -1) return 4;
  if (lower.indexOf("vanster") != -1) return 1;
  if (lower.indexOf("hoger") != -1) return 2;
  if (lower.indexOf("framme") != -1 || lower.indexOf("ankomst") != -1) return 7;

  return 0; // Default straight arrow
}

static int parseExitNumber(String instruction) {
  if (instruction.length() == 0) return 0;
  String lower = instruction;
  lower.toLowerCase();
  
  for (int i = 1; i <= 9; i++) {
    String p1 = String(i) + ":a";
    String p2 = String(i) + ":e";
    String p3 = String(i) + "st";
    String p4 = String(i) + "nd";
    String p5 = String(i) + "rd";
    String p6 = String(i) + "th";
    
    if (lower.indexOf(p1) != -1 || lower.indexOf(p2) != -1 || 
        lower.indexOf(p3) != -1 || lower.indexOf(p4) != -1 || 
        lower.indexOf(p5) != -1 || lower.indexOf(p6) != -1) {
      return i;
    }
  }
  return 0;
}

static void extractDistanceAndEta(String text, String subText, String &streetOut, String &distanceOut, String &etaOut) {
    streetOut = "";
    distanceOut = "";
    etaOut = "";
    text.trim();
    subText.trim();
    
    // Check if subText has an ETA/arrival time pattern (contains ":" or "Ankomst" / "Arriving")
    if (subText.indexOf(":") != -1 || subText.indexOf("Ankomst") != -1 || subText.indexOf("Arriving") != -1 || subText.indexOf("arrival") != -1) {
        etaOut = subText;
        if (text.indexOf("m") != -1 || text.indexOf("km") != -1 || text.indexOf("ft") != -1 || text.indexOf("mi") != -1) {
            distanceOut = text;
        } else {
            streetOut = text; // If text is not a distance, it's likely a street
        }
    } else {
        // Fallbacks if Google Maps formats things differently
        if (subText.indexOf("m") != -1 || subText.indexOf("km") != -1) {
            distanceOut = subText;
        }
        streetOut = text;
    }

    if (etaOut.length() == 0) {
        // Check if text holds ETA instead
        if (text.indexOf(":") != -1 && (text.indexOf("Ankomst") != -1 || text.indexOf("Arriving") != -1)) {
            etaOut = text;
            streetOut = "";
            distanceOut = subText;
        }
    }
}

void ble_process_nav_data() {
  std::lock_guard<std::mutex> lock(nav_mutex);
  if (g_new_nav_data) {
    g_new_nav_data = false;
    ui_show_navigation(g_nav_maneuver_id, g_nav_exit_number, g_nav_step_distance.c_str(), g_nav_eta.c_str(),
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
    ui_show_navigation(0, 0, "-", "--:--", "Connected. Waiting for route...", "00:00", 0);

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
      // Incoming format: title|text|subText|currentTime
      String payload = String(rxValue.c_str());

      int firstPipe = payload.indexOf('|');
      int secondPipe = payload.indexOf('|', firstPipe + 1);
      int thirdPipe = payload.indexOf('|', secondPipe + 1);

      if (firstPipe != -1 && secondPipe != -1 && thirdPipe != -1) {
        String title = payload.substring(0, firstPipe);
        String text = payload.substring(firstPipe + 1, secondPipe);
        String subText = payload.substring(secondPipe + 1, thirdPipe);
        String currentTime = payload.substring(thirdPipe + 1);

        transliterate(title);
        transliterate(text);
        transliterate(subText);

        String stepDistance = "";
        String street = "";
        String eta = "";

        // Pre-parse the android bundled texts in case it holds the distance
        extractDistanceAndEta(text, subText, street, stepDistance, eta);

        // Now actively parse distance from title (e.g. "190 m · Timotejvägen 25")
        // delimiter detection prioritizing wide to narrow matches
        int delimiterLen = 0;
        int delimiterIdx = title.indexOf(" \xC2\xB7 ");
        if (delimiterIdx != -1) {
            delimiterLen = 4;
        } else {
            delimiterIdx = title.indexOf(" - ");
            if (delimiterIdx != -1) {
                delimiterLen = 3;
            } else {
                delimiterIdx = title.indexOf("\xC2\xB7");
                if (delimiterIdx != -1) {
                    delimiterLen = 2;
                }
            }
        }

        String pureInstruction = title;

        if (delimiterIdx != -1) {
            String prefix = title.substring(0, delimiterIdx);
            prefix.trim();
            
            // Only overwrite if it actually looks like a prefix distance
            if (stepDistance.length() == 0 || prefix.length() > 0) {
                stepDistance = prefix;
            }

            pureInstruction = title.substring(delimiterIdx + delimiterLen);
            pureInstruction.trim();
            
            // Handle edgecases where trailing spaces or dash remains
            if (pureInstruction.startsWith("- ")) {
                pureInstruction = pureInstruction.substring(2);
            }
        }

        int maneuverId = parseManeuverId(pureInstruction);
        int exitNumber = parseExitNumber(pureInstruction);

        if (street.length() == 0 || street == title || street == pureInstruction) {
            street = pureInstruction;
        }

        // Calculate Route Progress if ETA contains a time
        int progress = 0;
        int etaColon = eta.indexOf(':');
        if (etaColon >= 2) {
          String etaStr = eta.substring(etaColon - 2, etaColon + 3);
          int etaMins = parseTimeToMinutes(etaStr);
          int curMins = parseTimeToMinutes(currentTime);

          if (etaMins != -1 && curMins != -1) {
            if (etaMins < curMins && (curMins - etaMins) > 12 * 60) {
              etaMins += 24 * 60;
            }
            int remaining = etaMins - curMins;
            if (remaining < 0) remaining = 0;

            if (g_max_trip_minutes < remaining) {
              g_max_trip_minutes = remaining;
            }

            if (g_max_trip_minutes > 0) {
              progress = 100 - ((remaining * 100) / g_max_trip_minutes);
            }
          }
        } else {
            g_max_trip_minutes = -1;
        }

        if (progress < 0) progress = 0;
        if (progress > 100) progress = 100;

        std::lock_guard<std::mutex> lock(nav_mutex);
        g_nav_maneuver_id = maneuverId;
        g_nav_exit_number = exitNumber;
        g_nav_instruction = title;
        g_nav_street = street;
        g_nav_step_distance = stepDistance;
        g_nav_eta = eta;
        g_nav_current_time = currentTime;
        g_nav_progress_percent = progress;
        g_new_nav_data = true;
      } else {
        // Fallback if parsing fails
        std::lock_guard<std::mutex> lock(nav_mutex);
        g_nav_maneuver_id = 0;
        g_nav_exit_number = 0;
        g_nav_instruction = "Nav";
        g_nav_street = "-";
        g_nav_step_distance = rxValue.c_str();
        g_nav_eta = "--:--";
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
            
            String street = "";
            String stepDist = "";
            String eta = "";
            extractDistanceAndEta(ancs_message, "", street, stepDist, eta); // For ANCS, ETA is sometimes mixed in message or not sent. We do our best.

            std::lock_guard<std::mutex> lock(nav_mutex);
            g_nav_instruction = ancs_title;
            g_nav_street = street;
            g_nav_step_distance = stepDist;
            g_nav_eta = eta;
            g_nav_maneuver_id = parseManeuverId(ancs_title);
            g_nav_exit_number = parseExitNumber(ancs_title);
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
