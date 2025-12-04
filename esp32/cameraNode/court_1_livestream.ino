#include <WiFi.h>
#include "esp_camera.h"
#include "esp_http_server.h"

// ===============================
// Camera Model: WROVER KIT
// ===============================
#define CAMERA_MODEL_WROVER_KIT
#include "camera_pins.h"   // Uses your exact WROVER pin map

// ===============================
// WiFi Credentials
// ===============================
const char* ssid = "joseph";
const char* password = "password";


// ===============================
// Stream Handler
// ===============================
static esp_err_t stream_handler(httpd_req_t *req) {
    camera_fb_t * fb = NULL;
    char buf[64];

    httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=frame");

    while (true) {
        fb = esp_camera_fb_get();

        if (!fb) {
            Serial.println("Camera capture failed!");
            return ESP_FAIL;
        }

        size_t hlen = snprintf(buf, sizeof(buf),
                               "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
                               fb->len);

        if (httpd_resp_send_chunk(req, buf, hlen) != ESP_OK ||
            httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len) != ESP_OK ||
            httpd_resp_send_chunk(req, "\r\n", 2) != ESP_OK) {

            esp_camera_fb_return(fb);
            break;
        }

        esp_camera_fb_return(fb);
    }

    return ESP_OK;
}


// ===============================
// Start Web Server
// ===============================
void startCameraServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 81;

    httpd_uri_t stream_uri = {
        .uri = "/stream",
        .method = HTTP_GET,
        .handler = stream_handler,
        .user_ctx = NULL
    };

    httpd_handle_t httpd_stream = NULL;

    if (httpd_start(&httpd_stream, &config) == ESP_OK) {
        httpd_register_uri_handler(httpd_stream, &stream_uri);
        Serial.println("Camera streaming server started on port 81");
    }
}


// ===============================
// Setup
// ===============================
void setup() {
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    Serial.println();
    Serial.println("Starting ESP32-WROVER Camera Server...");

    // CAMERA CONFIG (from your pin map)
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer  = LEDC_TIMER_0;

    config.pin_d0      = Y2_GPIO_NUM;
    config.pin_d1      = Y3_GPIO_NUM;
    config.pin_d2      = Y4_GPIO_NUM;
    config.pin_d3      = Y5_GPIO_NUM;
    config.pin_d4      = Y6_GPIO_NUM;
    config.pin_d5      = Y7_GPIO_NUM;
    config.pin_d6      = Y8_GPIO_NUM;
    config.pin_d7      = Y9_GPIO_NUM;

    config.pin_xclk    = XCLK_GPIO_NUM;
    config.pin_pclk    = PCLK_GPIO_NUM;
    config.pin_vsync   = VSYNC_GPIO_NUM;
    config.pin_href    = HREF_GPIO_NUM;

    config.pin_sccb_sda= SIOD_GPIO_NUM;
    config.pin_sccb_scl= SIOC_GPIO_NUM;

    config.pin_pwdn    = PWDN_GPIO_NUM;
    config.pin_reset   = RESET_GPIO_NUM;

    config.xclk_freq_hz= 20000000;
    config.pixel_format= PIXFORMAT_JPEG;
    config.frame_size  = FRAMESIZE_QVGA;
    config.jpeg_quality= 12;
    config.fb_count    = 2;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode   = CAMERA_GRAB_LATEST;

    Serial.println("Initializing camera...");
    esp_err_t err = esp_camera_init(&config);

    if (err != ESP_OK) {
        Serial.printf("Camera init failed! Error 0x%x\n", err);
        return;
    }

    Serial.println("Camera init successful!");

    // CONNECT TO WIFI
    Serial.printf("Connecting to WiFi: %s\n", ssid);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nWiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    // START STREAMING SERVER
    startCameraServer();
    Serial.println("Camera stream ready!");

    Serial.printf("Stream URL: http://%s:81/stream1\n",
        WiFi.localIP().toString().c_str());
}


// ===============================
// Loop
// ===============================
void loop() {
    delay(10);
}
