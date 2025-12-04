#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <esp_timer.h>

#include "board_config.h"   // selects camera model, pulls in camera_pins.h

// WiFi Configuration
// Update SSID/PASS depending on network
#define WIFI_SSID     ""
#define WIFI_PASS     ""

// Backend address/endpoint
#define BACKEND_HOST  "172.20.10.3"   // Change to backend IP address
#define BACKEND_PORT  8080
#define PATH_SNAPSHOT "/snapshot/court1"  // Endpoint that accepts a single JPEG

// How often to send a snapshot
#define SNAPSHOT_MS   1000

// Initialize camera
static void init_camera() {
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
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;
  config.frame_size   = FRAMESIZE_UXGA;        // sensor init default; will adjust below
  config.pixel_format = PIXFORMAT_JPEG;        // we want JPEG for upload
  config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location  = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;                    // lower is higher quality; 10–15 typical
  config.fb_count     = 1;                     // one FB is simpler/safer for snapshots

  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 12;
      config.fb_count = 2;                     // allow double buffering if PSRAM present
      config.grab_mode = CAMERA_GRAB_LATEST;   // drop old frames if behind
    } else {
      config.frame_size  = FRAMESIZE_SVGA;     // smaller if no PSRAM
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    config.frame_size = FRAMESIZE_240X240;
  }

  ESP_ERROR_CHECK(esp_camera_init(&config));

  // Set size and quality, keep it small for demo
  sensor_t *s = esp_camera_sensor_get();
  s->set_vflip(s, 1);
  s->set_hmirror(s, 0);
  //s->set_framesize(s, FRAMESIZE_QVGA);
  s->set_framesize(s, FRAMESIZE_VGA);
  s->set_quality(s, 12);
}

// POST a single JPEG to the backend
bool post_snapshot(const uint8_t *jpg, size_t len) {
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  WiFiClient client;

  String url = String("http://") + BACKEND_HOST + ":" + String(BACKEND_PORT) + PATH_SNAPSHOT;

  if (!http.begin(client, url)) {
    Serial.println("[snap] http.begin failed");
    return false;
  }

  http.addHeader("Content-Type", "image/jpeg");
  http.setTimeout(15000);

  // Library POST signature expects non-const
  int code = http.POST((uint8_t*)jpg, len);

  if (code <= 0) {
    // Negative/zero codes are transport errors
    Serial.printf("[snap] POST failed, err=%d\n", code);
    http.end();
    return false;
  }

  // Log status code and payload size for debugging
  Serial.printf("[snap] POST -> HTTP %d (bytes=%u)\n", code, (unsigned)len);
  http.end();
  return (code >= 200 && code < 300);
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.print("\nWiFi connected, IP: "); Serial.println(WiFi.localIP());
  WiFi.setSleep(false); // keep active to reduce latency

  // Camera
  init_camera();
}

void loop() {
  // Simple interval timer based on esp_timer
  static uint64_t last_snap = 0;
  uint64_t now_ms = esp_timer_get_time() / 1000ULL;

  if (now_ms - last_snap >= SNAPSHOT_MS) {
    // Capture a frame buffer
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("[snap] camera capture failed");
      delay(100);
      return;
    }

    // Ensure JPEG format, convert if necessary
    uint8_t *jpg = fb->buf;
    size_t   len = fb->len;
    uint8_t *malloc_jpg = nullptr;

    if (fb->format != PIXFORMAT_JPEG) {
      // Encode to JPEG with quality ~80 if the sensor delivered a raw format
      size_t out_len = 0;
      if (!frame2jpg(fb, 80, &malloc_jpg, &out_len)) {
        esp_camera_fb_return(fb);
        Serial.println("[snap] JPEG encode failed");
        return;
      }
      jpg = malloc_jpg;
      len = out_len;
    }

    // Send the snapshot to the backend
    bool ok = post_snapshot(jpg, len);

    // Free any temporary JPEG buffer we allocated
    if (malloc_jpg) { free(malloc_jpg); }
    // Return the camera framebuffer to the driver
    esp_camera_fb_return(fb);

    // Bump the timer so the next snapshot waits SNAPSHOT_MS
    last_snap = now_ms;

    if (!ok) {
      // Non-fatal: log and continue. Transient Wi-Fi/HTTP errors are expected sometimes.
      Serial.println("[snap] upload failed");
    }
  }

  delay(5); // small yield
}
