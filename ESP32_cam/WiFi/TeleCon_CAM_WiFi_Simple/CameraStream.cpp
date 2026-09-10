/*
 * CameraStream.cpp — SoftAP + MJPEG /stream + JPEG /capture (AI-Thinker).
 * GET /camconfig: runtime Smooth / Balanced / HQ (same Android contract as Binary).
 * SoftAP PS_NONE for video budget.
 */

#include "CameraStream.h"
#include "CameraStreamCamConfig.h"
#include "DebugConfig.h"

#include <WiFi.h>
#include <WebServer.h>
#include "esp_camera.h"
#include "esp_wifi.h"

namespace {

CameraStreamHttpServer server(80);
bool cameraReady = false;
bool serverStarted = false;
CameraStreamIdleHook idleHook = nullptr;
bool streamActive = false;

constexpr int CAM_PIN_PWDN = 32;
constexpr int CAM_PIN_RESET = -1;
constexpr int CAM_PIN_XCLK = 0;
constexpr int CAM_PIN_SIOD = 26;
constexpr int CAM_PIN_SIOC = 27;
constexpr int CAM_PIN_D7 = 35;
constexpr int CAM_PIN_D6 = 34;
constexpr int CAM_PIN_D5 = 39;
constexpr int CAM_PIN_D4 = 36;
constexpr int CAM_PIN_D3 = 21;
constexpr int CAM_PIN_D2 = 19;
constexpr int CAM_PIN_D1 = 18;
constexpr int CAM_PIN_D0 = 5;
constexpr int CAM_PIN_VSYNC = 25;
constexpr int CAM_PIN_HREF = 23;
constexpr int CAM_PIN_PCLK = 22;

void runIdleHook() {
  if (idleHook) idleHook();
  yield();
}

/** Cooperative wait — delay() blocks TCP RC on the Arduino loop task. */
void waitMsCoop(uint32_t ms) {
  const uint32_t start = millis();
  while ((millis() - start) < ms) {
    yield();
  }
}

constexpr size_t kJpegWriteChunk = 8192;

bool writeBufChunked(WiFiClient& client, const uint8_t* data, size_t len) {
  size_t off = 0;
  while (off < len) {
    if (!client.connected()) return false;
    const size_t n = (len - off > kJpegWriteChunk) ? kJpegWriteChunk : (len - off);
    if (client.write(data + off, n) != n) return false;
    off += n;
    runIdleHook();
  }
  return true;
}

void waitUntilMs(WiFiClient& client, uint32_t deadlineMs, bool* ok) {
  while (client.connected()) {
    if (static_cast<int32_t>(millis() - deadlineMs) >= 0) break;
    runIdleHook();
    if (!cameraStreamCamConfigPollWhileStreaming(server, cameraReady, client.connected())) {
      *ok = false;
      break;
    }
  }
}

bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = CAM_PIN_D0;
  config.pin_d1 = CAM_PIN_D1;
  config.pin_d2 = CAM_PIN_D2;
  config.pin_d3 = CAM_PIN_D3;
  config.pin_d4 = CAM_PIN_D4;
  config.pin_d5 = CAM_PIN_D5;
  config.pin_d6 = CAM_PIN_D6;
  config.pin_d7 = CAM_PIN_D7;
  config.xclk_freq_hz = 20000000;
  config.pin_xclk = CAM_PIN_XCLK;
  config.pin_pclk = CAM_PIN_PCLK;
  config.pin_vsync = CAM_PIN_VSYNC;
  config.pin_href = CAM_PIN_HREF;
  config.pin_sccb_sda = CAM_PIN_SIOD;
  config.pin_sccb_scl = CAM_PIN_SIOC;
  config.pin_pwdn = CAM_PIN_PWDN;
  config.pin_reset = CAM_PIN_RESET;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = cameraStreamGetFramesize();
  config.jpeg_quality = cameraStreamGetQuality();
  config.fb_count = 1;
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
  waitMsCoop(50);
  WiFi.mode(WIFI_OFF);
  waitMsCoop(50);
  WiFi.mode(WIFI_AP);
  waitMsCoop(50);

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
  client.setNoDelay(true);
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
      const uint32_t retryDeadline = millis() + 10;
      bool retryOk = true;
      waitUntilMs(client, retryDeadline, &retryOk);
      if (!retryOk) break;
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
              writeBufChunked(client, fb->buf, fb->len) &&
              client.print("\r\n") > 0;
    esp_camera_fb_return(fb);

    if (!ok || !client.connected()) break;

    const uint32_t periodMs = cameraStreamFpsPeriodMs();
    if (periodMs > 0) {
      waitUntilMs(client, frameStartMs + periodMs, &ok);
      if (!ok) break;
    }
  }
  streamActive = false;
  cameraStreamCamConfigNoteStreamActive(false);
}

void handleRoot() {
  server.send(200, "text/plain",
              "TeleCon RC Vehicle Pro — CAM Wi‑Fi Simple\n"
              "GET /stream  → MJPEG\n"
              "GET /capture → JPEG\n"
              "GET /camconfig → Smooth/Balanced/HQ (needs 2026-08+ firmware)\n"
              "TCP :3333 → RC:CONNECT,proto,simple\n");
}

}  // namespace

void cameraStreamSetIdleHook(CameraStreamIdleHook hook) {
  idleHook = hook;
}

void cameraStreamBegin(const char* apSsid, const char* apPass) {
  if (serverStarted) return;

  cameraStreamCamConfigInit("qqvga", 28, 8);  // Android RC Vehicle Smooth
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

bool cameraStreamIsActive() {
  return streamActive;
}
