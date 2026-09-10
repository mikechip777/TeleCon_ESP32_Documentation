/*
 * CameraStream.cpp — SoftAP + MJPEG /stream + JPEG /capture (ESP32-CAM).
 * Video-only sketch: no SoftAP TCP control on this board.
 * GET /camconfig: runtime Smooth / Balanced / HQ.
 * SoftAP PS_NONE for video budget.
 *
 * Camera sensor pins come from CAMERA_MODEL_* + camera_pins.h (board model).
 * Do not use 21/22/26/27 for user I2C. GPIO 1/3 = Serial Monitor, not user UART.
 */

#include "CameraStream.h"
#include "CameraStreamCamConfig.h"
#include "DebugConfig.h"

#include <WiFi.h>
#include <WebServer.h>
#include "esp_camera.h"
#include "esp_wifi.h"

#define CAMERA_MODEL_AI_THINKER
// #define CAMERA_MODEL_WROVER_KIT
// #define CAMERA_MODEL_ESP_EYE
// #define CAMERA_MODEL_M5STACK_PSRAM
// #define CAMERA_MODEL_M5STACK_WIDE
// #define CAMERA_MODEL_TTGO_T_JOURNAL
#include "camera_pins.h"

namespace {

CameraStreamHttpServer server(80);
bool cameraReady = false;
bool serverStarted = false;
CameraStreamIdleHook idleHook = nullptr;
bool streamActive = false;

void runIdleHook() {
  if (idleHook) idleHook();
  yield();
}

bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = cameraStreamGetFramesize();
  config.jpeg_quality = cameraStreamGetQuality();
  config.fb_count = 2;
#if defined(CAMERA_GRAB_LATEST)
  config.grab_mode = CAMERA_GRAB_LATEST;
#endif

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    DBG_PRINTF("Camera init failed: 0x%x\n", err);
    return false;
  }
  return true;
}

bool startSoftAp(const char* apSsid, const char* apPass) {
  WiFi.persistent(false);
  WiFi.disconnect(true, true);
  delay(50);
  WiFi.mode(WIFI_OFF);
  delay(50);
  WiFi.mode(WIFI_AP);
  delay(50);

  IPAddress apIP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, gateway, subnet);

  bool apOk = WiFi.softAP(apSsid, apPass, 1, 0, 4);
  esp_wifi_set_ps(WIFI_PS_NONE);

  DBG_PRINTF("WiFi SoftAP %s  SSID=%s  IP=%s\n",
                apOk ? "OK" : "FAILED",
                apSsid,
                WiFi.softAPIP().toString().c_str());
  return apOk;
}

void addCaptureHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.sendHeader("Connection", "close");
}

void handleCapture() {
  if (!cameraReady) {
    addCaptureHeaders();
    server.send(503, "text/plain", "Camera not available");
    return;
  }
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    addCaptureHeaders();
    server.send(500, "text/plain", "Capture failed");
    return;
  }
  addCaptureHeaders();
  server.send_P(200, "image/jpeg", (const char*)fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

void handleStream() {
  if (!cameraReady) {
    addCaptureHeaders();
    server.send(503, "text/plain", "Camera not available");
    return;
  }
  if (streamActive) {
    addCaptureHeaders();
    server.send(503, "text/plain", "stream busy (single client)");
    return;
  }

  WiFiClient client = server.client();
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: multipart/x-mixed-replace; boundary=frame"));
  client.println(F("Access-Control-Allow-Origin: *"));
  client.println(F("Cache-Control: no-cache, no-store, must-revalidate"));
  client.println(F("Pragma: no-cache"));
  client.println(F("Connection: close"));
  client.println();

  streamActive = true;
  cameraStreamCamConfigNoteStreamActive(true);
  while (client.connected()) {
    const uint32_t frameStartMs = millis();
    runIdleHook();
    if (!cameraStreamCamConfigPollWhileStreaming(server, cameraReady, client.connected())) {
      break;
    }

    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
      delay(10);
      continue;
    }

    char hdr[96];
    const int hdrLen = snprintf(
        hdr, sizeof(hdr),
        "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
        static_cast<unsigned>(fb->len));
    bool ok = hdrLen > 0 &&
              client.write(reinterpret_cast<const uint8_t*>(hdr),
                           static_cast<size_t>(hdrLen)) ==
                  static_cast<size_t>(hdrLen) &&
              client.write(fb->buf, fb->len) == fb->len &&
              client.print("\r\n") > 0;
    esp_camera_fb_return(fb);

    if (!ok || !client.connected()) break;

    const uint32_t periodMs = cameraStreamFpsPeriodMs();
    if (periodMs > 0) {
      while (client.connected()) {
        const uint32_t elapsed = millis() - frameStartMs;
        if (elapsed >= periodMs) break;
        runIdleHook();
        if (!cameraStreamCamConfigPollWhileStreaming(server, cameraReady, client.connected())) {
          ok = false;
          break;
        }
        const uint32_t remain = periodMs - elapsed;
        delay(remain > 5 ? 5 : remain);
      }
      if (!ok) break;
    }
  }
  streamActive = false;
  cameraStreamCamConfigNoteStreamActive(false);
}

void handleRoot() {
  server.send(200, "text/plain",
              "TeleCon Control Panel — CAM SoftAP Video (camera only)\n"
              "GET /stream  -> MJPEG\n"
              "GET /capture -> JPEG\n"
              "GET /camconfig -> Smooth/Balanced/HQ (needs 2026-08+ firmware)\n"
              "No TCP :3333 — control is on a separate DevKit Bluetooth sketch\n");
}

}  // namespace

void cameraStreamSetIdleHook(CameraStreamIdleHook hook) {
  idleHook = hook;
}

void cameraStreamBegin(const char* apSsid, const char* apPass) {
  if (serverStarted) return;

  cameraStreamCamConfigInit("vga", 15, 12);  // Android Control Panel Balanced
  cameraStreamCamConfigSetSoftApCredentials(apSsid, apPass);
  startSoftAp(apSsid, apPass);
  cameraReady = initCamera();
  DBG_PRINTF("Camera sensor %s\n", cameraReady ? "OK" : "FAILED");

  server.on("/", handleRoot);
  server.on("/stream", HTTP_GET, handleStream);
  server.on("/capture", HTTP_GET, handleCapture);
  cameraStreamCamConfigAttach(server, cameraReady);
  server.begin();
  serverStarted = true;
}

void cameraStreamPoll() {
  if (!serverStarted) return;
  server.handleClient();
}

bool cameraStreamReady() {
  return serverStarted && cameraReady;
}
