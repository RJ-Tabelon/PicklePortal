/*  Court Node
 *  - LCD1602 I2C at 0x27 (SDA=21, SCL=22)
 *  - Photoresistors on GPIO 32, 33, 34 (ADC1)
 *  - Status LEDs: GREEN=25, YELLOW=26, RED=27
 *
 *  Functionality:
 *    - Determine LDR baselines at boot (no paddles on pads).
 *    - queueLen = number of covered pads.
 *    - LED logic:
 *        AVAILABLE            (players=0, queue=0)         -> GREEN
 *        OCCUPIED_NO_QUEUE    (players>0, queue=0)         -> YELLOW
 *        OCCUPIED_WITH_QUEUE  (queue>0)                    -> RED
 *        OFFLINE (no server)                               -> YELLOW slow blink
 *    - LCD:
 *        Line1: "Players: <n>"
 *        Line2: "Queue:   <m>"
 */

#include <WiFi.h>
#include <WebSocketsClient.h>      // from arduinoWebSockets
#include <ArduinoJson.h>           // ArduinoJson
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// WiFi Config
#define WIFI_SSID       ""
#define WIFI_PASS       ""

// WebSocket endpoint
const char* WS_HOST   = "192.168.1.50";  // backend IP / host
const uint16_t WS_PORT = 8080;
const char* WS_PATH   = "/ws/courts/1";  // Court path
const char* COURT_ID  = "court-1";

// LCD
#define LCD_ADDR 0x27
#define LCD_COLS 16
#define LCD_ROWS 2

// Pins
const int LED_G = 25;
const int LED_Y = 26;
const int LED_R = 27;

const int LDR_PINS[] = { 32, 33, 34 };   // ADC1 only
const size_t LDR_COUNT = sizeof(LDR_PINS) / sizeof(LDR_PINS[0]);

// Calibration / detection
const uint32_t CALIBRATION_MS = 1500; 
const float EMA_ALPHA = 0.20f;
const int MIN_DELTA = 140;
const int HYSTERESIS = 60;

// Update timing
const uint32_t UI_REFRESH_MS = 200;
const uint32_t HB_MS         = 10000;
const uint32_t WS_RETRY_MS   = 5000;

// Globals
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);
WebSocketsClient ws;

int playerCount = 0;            // updated from backend
int queueLen    = 0;            // computed from LDRs
int lastQueueLen = -1;

bool serverOnline = false;

int32_t baseline[LDR_COUNT];    // ambient average at boot
float   emaVal[LDR_COUNT];      // smoothed ADC value
bool    padCovered[LDR_COUNT];  // current covered state

uint32_t t_ui = 0, t_hb = 0, t_ws = 0;

// Utility functions
String iso8601() {
  // Simple ISO-like timestamp (no TZ)
  char buf[32];
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &t);
  return String(buf);
}

void setLEDs(bool g, bool y, bool r) {
  digitalWrite(LED_G, g);
  digitalWrite(LED_Y, y);
  digitalWrite(LED_R, r);
}

enum CourtState {
  AVAILABLE,
  OCCUPIED_NO_QUEUE,
  OCCUPIED_WITH_QUEUE,
  OFFLINE
};

CourtState computeState(int players, int q, bool online) {
  if (!online) return OFFLINE;
  if (q > 0) return OCCUPIED_WITH_QUEUE;
  if (players > 0) return OCCUPIED_NO_QUEUE;
  return AVAILABLE;
}

void renderLEDs(CourtState s) {
  static uint32_t t_blink = 0;
  static bool blink = false;

  switch (s) {
    case AVAILABLE:           setLEDs(true,  false, false); break;   // green
    case OCCUPIED_NO_QUEUE:   setLEDs(false, true,  false); break;   // yellow
    case OCCUPIED_WITH_QUEUE: setLEDs(false, false, true ); break;   // red
    case OFFLINE:
      if (millis() - t_blink > 600) { blink = !blink; t_blink = millis(); }
      setLEDs(false, blink, false);                                   // yellow blink
      break;
  }
}

void renderLCD() {
  lcd.setCursor(0, 0);
  char line1[17];
  snprintf(line1, sizeof(line1), "Players:%4d", playerCount);
  lcd.print(line1);
  int len1 = strlen(line1);
  for (int i = len1; i < LCD_COLS; ++i) lcd.print(' ');

  lcd.setCursor(0, 1);
  char line2[17];
  snprintf(line2, sizeof(line2), "Queue:  %4d", queueLen);
  lcd.print(line2);
  int len2 = strlen(line2);
  for (int i = len2; i < LCD_COLS; ++i) lcd.print(' ');
}

void publishQueueUpdate() {
  if (!serverOnline) return;

  StaticJsonDocument<256> doc;
  doc["type"] = "queueUpdate";
  doc["courtId"] = COURT_ID;
  doc["queueLen"] = queueLen;
  doc["ts"] = iso8601();

  String out;
  serializeJson(doc, out);
  ws.sendTXT(out);
}

void publishHeartbeat() {
  if (!serverOnline) return;

  StaticJsonDocument<256> doc;
  doc["type"]  = "heartbeat";
  doc["courtId"] = COURT_ID;
  doc["rssi"] = WiFi.RSSI();
  doc["ts"]   = iso8601();

  String out;
  serializeJson(doc, out);
  ws.sendTXT(out);
}

// LDR handling
void calibrateLDRs() {
  // Assumes pads are empty during boot.
  uint32_t start = millis();
  int64_t sum[LDR_COUNT] = {0};
  uint32_t n = 0;

  while (millis() - start < CALIBRATION_MS) {
    for (size_t i = 0; i < LDR_COUNT; ++i) {
      sum[i] += analogRead(LDR_PINS[i]);
    }
    n++;
    delay(5);
  }
  for (size_t i = 0; i < LDR_COUNT; ++i) {
    baseline[i] = sum[i] / (int32_t)n;
    emaVal[i]   = baseline[i];
    padCovered[i] = false;
  }
}

void updateQueueFromLDRs() {
  int active = 0;
  for (size_t i = 0; i < LDR_COUNT; ++i) {
    int raw = analogRead(LDR_PINS[i]);
    emaVal[i] = EMA_ALPHA * raw + (1.0f - EMA_ALPHA) * emaVal[i];

    // Decide covered/uncovered relative to baseline
    // Assumption: covering LDR makes it darker -> ADC reading moves away from baseline.
    int delta = baseline[i] - (int)emaVal[i]; // positive if darker than baseline

    int onThresh  = MIN_DELTA;                 // enter "covered" when delta >= onThresh
    int offThresh = MIN_DELTA - HYSTERESIS;    // leave "covered" when delta < offThresh

    if (!padCovered[i]) {
      if (delta >= onThresh) padCovered[i] = true;
    } else {
      if (delta < offThresh) padCovered[i] = false;
    }
    if (padCovered[i]) active++;
  }
  queueLen = active;
}

// WiFi / WebSocket
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 12000) {
    delay(250);
  }
}

void wsEventCb(WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      serverOnline = true;
      // Announce presence
      {
        StaticJsonDocument<256> doc;
        doc["type"] = "hello";
        doc["courtId"] = COURT_ID;
        String out; serializeJson(doc, out);
        ws.sendTXT(out);
      }
      break;

    case WStype_DISCONNECTED:
      serverOnline = false;
      break;

    case WStype_TEXT:
      // Expect messages like: {"playerCount": 2, "courtState": "..."}
      if (length > 0) {
        StaticJsonDocument<256> doc;
        DeserializationError err = deserializeJson(doc, payload, length);
        if (!err) {
          if (doc.containsKey("playerCount")) {
            int newPc = doc["playerCount"].as<int>();
            playerCount = max(0, newPc);
          }
        }
      }
      break;

    default:
      break;
  }
}

void ensureWebSocket() {
  static bool initialized = false;

  if (!initialized) {
    ws.begin(WS_HOST, WS_PORT, WS_PATH);
    // ws.setAuthorization("user","pass"); // Might be needed
    ws.onEvent(wsEventCb);
    ws.setReconnectInterval(WS_RETRY_MS);
    initialized = true;
  }
  ws.loop();
}

// Setup
void setup() {
  // GPIO
  pinMode(LED_G, OUTPUT);
  pinMode(LED_Y, OUTPUT);
  pinMode(LED_R, OUTPUT);
  setLEDs(false,false,false);

  // ADC
  for (size_t i = 0; i < LDR_COUNT; ++i) {
    pinMode(LDR_PINS[i], INPUT);
  }
  analogReadResolution(12);

  // LCD
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Court Status Node");
  lcd.setCursor(0,1); lcd.print("Calibrating...");

  // Wi-Fi
  connectWiFi();

  // Calibrate LDR baselines
  calibrateLDRs();

  // WebSocket will be started in loop() via ensureWebSocket()
  t_ui = t_hb = t_ws = millis();
}

void loop() {
  // Update sensors -> queueLen
  updateQueueFromLDRs();

  // Networking
  if (WiFi.status() == WL_CONNECTED) {
    ensureWebSocket();
  } else {
    serverOnline = false;
    static uint32_t t_reconnect = 0;
    if (millis() - t_reconnect > 3000) {
      connectWiFi();
      t_reconnect = millis();
    }
  }

  // Publish queue change
  if (queueLen != lastQueueLen) {
    lastQueueLen = queueLen;
    publishQueueUpdate();
  }

  if (millis() - t_hb > HB_MS) {
    publishHeartbeat();
    t_hb = millis();
  }

  // UI refresh (LCD + LEDs)
  if (millis() - t_ui > UI_REFRESH_MS) {
    renderLCD();
    renderLEDs(computeState(playerCount, queueLen, serverOnline));
    t_ui = millis();
  }
}
