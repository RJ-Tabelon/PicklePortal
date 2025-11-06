/*
  Camera node
  - Local preview at http://<ip> (snapshot endpoint)
  - Push MJPEG to backend over persistent HTTP (multipart/x-mixed-replace)
  - Periodic heartbeat POST with RSSI/uptime/FPS
  - Robust reconnects

  Fill these before flashing:
    WIFI_SSID / WIFI_PASS
    BACKEND_HOST / BACKEND_PORT
    COURT_ID (used in paths)
*/

#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>   // from ESP32 core
#include <WiFiClient.h>
#include <esp_timer.h>

#define WIFI_SSID   "YOUR_SSID"
#define WIFI_PASS   "YOUR_PASSWORD"

const char* BACKEND_HOST = "192.168.1.50";
const uint16_t BACKEND_PORT = 8080;
const char* COURT_ID = "court-1";

// Endpoints on backend
String PATH_INGEST    = String("/ingest/") + COURT_ID;                     // persistent POST with MJPEG
String PATH_HEARTBEAT = String("/api/courts/") + COURT_ID + "/heartbeat";  // short POST JSON

// Camera tuning
framesize_t FRAME_SIZE = FRAMESIZE_VGA;
int JPEG_QUALITY       = 12;
int TARGET_FPS         = 12;

// Pin map
#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM     21
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       19
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM        5
#define Y2_GPIO_NUM        4
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Globals
WebServer web(80);
WiFiClient ingestClient;

const char* BOUNDARY = "esp32cam_wrover_mjpeg_boundary";
uint32_t lastHeartbeatMs = 0;
uint32_t heartbeatEveryMs = 10000;

uint32_t frameCount = 0;
uint32_t lastFpsWindowStart = 0;
float    rollingFps = 0.0;

uint32_t reconnectBackoffMs = 2000;
const uint32_t reconnectBackoffMax = 15000;

// Utiliy functions
static void wifiConnect() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(250);
  }
}

static bool ensureIngestConnected() {
  if (ingestClient.connected()) return true;

  if (!ingestClient.connect(BACKEND_HOST, BACKEND_PORT)) {
    return false;
  }
  // Open a persistent multipart MJPEG POST
  String hdr;
  hdr  = "POST " + PATH_INGEST + " HTTP/1.1\r\n";
  hdr += "Host: " + String(BACKEND_HOST) + ":" + String(BACKEND_PORT) + "\r\n";
  hdr += "Connection: keep-alive\r\n";
  hdr += "Content-Type: multipart/x-mixed-replace; boundary=" + String(BOUNDARY) + "\r\n";
  hdr += "\r\n";
  ingestClient.print(hdr);

  uint32_t t0 = millis();
  while (!ingestClient.available() && millis() - t0 < 1500) delay(10);
  if (ingestClient.available()) {
    (void)ingestClient.readStringUntil('\n'); 
  }
  return true;
}

static void sendHeartbeat() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClient hb;
  if (!hb.connect(BACKEND_HOST, BACKEND_PORT)) return;

  char body[256];
  snprintf(body, sizeof(body),
           "{\"courtId\":\"%s\",\"rssi\":%d,\"uptime\":%lu,\"fps\":%.1f}",
           COURT_ID, WiFi.RSSI(), (unsigned long)(millis()/1000), rollingFps);

  String req;
  req  = "POST " + PATH_HEARTBEAT + " HTTP/1.1\r\n";
  req += "Host: " + String(BACKEND_HOST) + ":" + String(BACKEND_PORT) + "\r\n";
  req += "Content-Type: application/json\r\n";
  req += "Connection: close\r\n";
  req += "Content-Length: " + String(strlen(body)) + "\r\n\r\n";

  hb.print(req);
  hb.print(body);

  // drain quickly
  uint32_t t0 = millis();
  while (hb.connected() && millis() - t0 < 1000) {
    while (hb.available()) hb.read();
    delay(10);
  }
  hb.stop();
}

// Camera
static bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;

  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;

  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;

  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;

  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;      // 20 MHz
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size   = FRAME_SIZE;  // VGA default for WROVER + PSRAM
    config.jpeg_quality = JPEG_QUALITY;
    config.fb_count     = 2;
  } else {
    config.frame_size   = FRAMESIZE_QVGA;
    config.jpeg_quality = 18;
    config.fb_count     = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) return false;

  return true;
}

// Local preview server
static void handleRoot() {
  String page =
    "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<style>body{font-family:system-ui;margin:20px}img{max-width:100%;height:auto}</style>"
    "</head><body><h3>ESP32-WROVER-KIT Camera Preview</h3>"
    "<p><a href='/snap'>Snapshot</a> (click to refresh)</p>"
    "<img src='/snap'/>"
    "</body></html>";
  web.send(200, "text/html", page);
}

static void handleSnap() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) { web.send(503, "text/plain", "Camera capture failed"); return; }
  web.send_P(200, "image/jpeg", (const char*)fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

// Streaming helpers
static bool sendMultipartFrame(WiFiClient& c, camera_fb_t* fb) {
  if (!c.connected()) return false;

  String hdr;
  hdr.reserve(128);
  hdr += "--"; hdr += BOUNDARY; hdr += "\r\n";
  hdr += "Content-Type: image/jpeg\r\n";
  hdr += "Content-Length: " + String(fb->len) + "\r\n\r\n";

  if (c.print(hdr) != (int)hdr.length()) return false;
  size_t wrote = c.write(fb->buf, fb->len);
  if (wrote != fb->len) return false;
  if (c.print("\r\n") != 2) return false;

  return true;
}

static void updateRollingFps() {
  uint32_t now = millis();
  if (lastFpsWindowStart == 0) {
    lastFpsWindowStart = now;
    frameCount = 0;
  }
  frameCount++;
  if (now - lastFpsWindowStart >= 2000) { // 2s window
    rollingFps = (1000.0f * frameCount) / (float)(now - lastFpsWindowStart);
    frameCount = 0;
    lastFpsWindowStart = now;
  }
}

// Setup
void setup() {
  Serial.begin(115200);
  delay(200);

  if (!initCamera()) {
    Serial.println("Camera init failed — rebooting...");
    delay(2000);
    ESP.restart();
  }

  wifiConnect();
  Serial.print("WiFi IP: "); Serial.println(WiFi.localIP());

  web.on("/", handleRoot);
  web.on("/snap", handleSnap);
  web.begin();

  lastHeartbeatMs = millis();
  lastFpsWindowStart = 0;
}

void loop() {
  web.handleClient();

  // Ensure Wi-Fi
  if (WiFi.status() != WL_CONNECTED) {
    uint32_t t0 = millis();
    wifiConnect();
    if (millis() - t0 > 0) delay(100);
    return;
  }

  // Ensure ingest connection
  if (!ensureIngestConnected()) {
    delay(reconnectBackoffMs);
    reconnectBackoffMs = min(reconnectBackoffMax, reconnectBackoffMs + 1000);
    return;
  } else {
    reconnectBackoffMs = 2000;
  }

  // Capture a frame
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) { delay(20); return; }

  uint32_t frameStart = millis();

  // Send as one MJPEG part
  bool ok = sendMultipartFrame(ingestClient, fb);
  esp_camera_fb_return(fb);

  if (!ok) {
    ingestClient.stop(); // force reconnect next loop
    delay(100);
    return;
  }

  updateRollingFps();

  // Heartbeat
  uint32_t now = millis();
  if (now - lastHeartbeatMs >= heartbeatEveryMs) {
    sendHeartbeat();
    lastHeartbeatMs = now;
  }

  // Pace FPS
  if (TARGET_FPS > 0) {
    uint32_t took = millis() - frameStart;
    uint32_t minFrameMs = 1000 / (uint32_t)TARGET_FPS;
    if (took < minFrameMs) delay(minFrameMs - took);
  }
}
