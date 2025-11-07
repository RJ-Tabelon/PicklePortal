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

enum CourtState {
  AVAILABLE,
  OCCUPIED_NO_QUEUE,
  OCCUPIED_WITH_QUEUE,
  OFFLINE
};

#include <WiFi.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

// WiFI Config
#define WIFI_SSID  "joseph"
#define WIFI_PASS  "password"

// Firebase Config
#define API_KEY "add-api-key-here"
#define DATABASE_URL "add-database-url-here"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// LCD Confuig
#define LCD_ADDR 0x27
#define LCD_COLS 16
#define LCD_ROWS 2
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// GPIO Pins
const int LED_G = 25;
const int LED_Y = 26;
const int LED_R = 27;
const int LDR_PINS[] = { 32, 33, 34 };
const size_t LDR_COUNT = sizeof(LDR_PINS) / sizeof(LDR_PINS[0]);

// Detection + Timers
const uint32_t CALIBRATION_MS = 1500;
const float EMA_ALPHA = 0.20f;
const int MIN_DELTA = 140;
const int HYSTERESIS = 60;
const uint32_t UI_REFRESH_MS = 200;

// Globals
int playerCount = 0;
int queueLen = 0;
int lastQueueLen = -1;
bool serverOnline = true;
int32_t baseline[LDR_COUNT];
float emaVal[LDR_COUNT];
bool padCovered[LDR_COUNT];
uint32_t t_ui = 0;

// Utility
void setLEDs(bool g, bool y, bool r) {
  digitalWrite(LED_G, g);
  digitalWrite(LED_Y, y);
  digitalWrite(LED_R, r);
}

CourtState computeState(int players, int q, bool online) {
  if (!online) return OFFLINE;
  if (q > 0) return OCCUPIED_WITH_QUEUE;
  if (players > 0) return OCCUPIED_NO_QUEUE;
  return AVAILABLE;
}

void renderLEDs(CourtState s) {
  static uint32_t t_blink = 0;
  static bool blink = false;

  if (s == AVAILABLE) setLEDs(true, false, false);
  else if (s == OCCUPIED_NO_QUEUE) setLEDs(false, true, false);
  else if (s == OCCUPIED_WITH_QUEUE) setLEDs(false, false, true);
  else if (s == OFFLINE) {
    if (millis() - t_blink > 600) { blink = !blink; t_blink = millis(); }
    setLEDs(false, blink, false);
  }
}

void renderLCD() {
  lcd.setCursor(0, 0);
  lcd.printf("Players:%4d", playerCount);
  lcd.setCursor(0, 1);
  lcd.printf("Queue:  %4d", queueLen);
}

void calibrateLDRs() {
  uint32_t start = millis();
  int64_t sum[LDR_COUNT] = {0};
  uint32_t n = 0;

  while (millis() - start < CALIBRATION_MS) {
    for (size_t i = 0; i < LDR_COUNT; ++i)
      sum[i] += analogRead(LDR_PINS[i]);
    n++;
    delay(5);
  }

  for (size_t i = 0; i < LDR_COUNT; ++i) {
    baseline[i] = sum[i] / (int32_t)n;
    emaVal[i] = baseline[i];
    padCovered[i] = false;
  }
}

void updateQueueFromLDRs() {
  int active = 0;
  for (size_t i = 0; i < LDR_COUNT; ++i) {
    int raw = analogRead(LDR_PINS[i]);
    emaVal[i] = EMA_ALPHA * raw + (1.0f - EMA_ALPHA) * emaVal[i];
    int delta = baseline[i] - (int)emaVal[i];
    int onThresh = MIN_DELTA;
    int offThresh = MIN_DELTA - HYSTERESIS;

    if (!padCovered[i] && delta >= onThresh) padCovered[i] = true;
    else if (padCovered[i] && delta < offThresh) padCovered[i] = false;

    if (padCovered[i]) active++;
  }
  queueLen = active;
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  Serial.print(" Connected! IP: ");
  Serial.println(WiFi.localIP());
}

// Updates the firebase database in real time. Writes values from the photoresistor to the firebase and retreives the playerCount from the database.
void updateFirebase(int queue, CourtState state) {
  if (!Firebase.ready()) return;

  String base = "/court1";
  String statusStr, colorStr;

  if (state == AVAILABLE) {
    statusStr = "available"; colorStr = "green";
  } else if (state == OCCUPIED_NO_QUEUE) {
    statusStr = "occupied_no_queue"; colorStr = "yellow";
  } else if (state == OCCUPIED_WITH_QUEUE) {
    statusStr = "occupied_with_queue"; colorStr = "red";
  } else {
    statusStr = "offline"; colorStr = "yellow";
  }

  Serial.println("Updating Firebase...");
  Firebase.RTDB.setInt(&fbdo, base + "/queueSize", queue);
  if (Firebase.RTDB.getInt(&fbdo, base + "/occupancyCount")) {
  if (fbdo.dataType() == "int") {
    playerCount = fbdo.intData();  // 
    Serial.print("Player count from Firebase: ");
    Serial.println(playerCount);
  }
} else {
  Serial.print("Failed to get occupancyCount: ");
  Serial.println(fbdo.errorReason());
}
  Firebase.RTDB.setString(&fbdo, base + "/status", statusStr);
  Firebase.RTDB.setString(&fbdo, base + "/lightColor", colorStr);
}

// Setup
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Court Node ===");

  pinMode(LED_G, OUTPUT);
  pinMode(LED_Y, OUTPUT);
  pinMode(LED_R, OUTPUT);
  setLEDs(false, false, false);

  for (size_t i = 0; i < LDR_COUNT; ++i)
    pinMode(LDR_PINS[i], INPUT);

  analogReadResolution(12);
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("Court Status Node");
  lcd.setCursor(0, 1);
  lcd.print("Calibrating...");

  connectWiFi();
  calibrateLDRs();

  // Firebase setup
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  Firebase.reconnectWiFi(true);

  Serial.println("Connecting to Firebase...");
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase anonymous login success");
  } else {
    Serial.printf("Firebase signUp failed: %s\n", config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);
  delay(2000);

  if (Firebase.ready())
    Serial.println("Firebase Ready!");
  else
    Serial.println("Firebase not ready!");

  lcd.clear();
  lcd.print("Ready!");
  delay(1000);

  t_ui = millis();
}

// Loop
void loop() {
  updateQueueFromLDRs();

  if (queueLen != lastQueueLen) {
    lastQueueLen = queueLen;
    Serial.print("Queue changed: ");
    Serial.println(queueLen);
  }

  if (millis() - t_ui > UI_REFRESH_MS) {
    renderLCD();
    renderLEDs(computeState(playerCount, queueLen, serverOnline));
    t_ui = millis();
  }
  CourtState cs = computeState(playerCount, queueLen, serverOnline);
  updateFirebase(queueLen, cs);
}
