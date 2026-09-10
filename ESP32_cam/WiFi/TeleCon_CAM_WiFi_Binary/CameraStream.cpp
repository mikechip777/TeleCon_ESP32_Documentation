#include "CameraStream.h"
#include "CameraStreamCamConfig.h"
#include "TeleConConfig.h"
#include "TeleConDebug.h"

#include <WiFi.h>
#include <WebServer.h>
#include "esp_camera.h"
#include "esp_wifi.h"
#include <cstring>

namespace {

CameraStreamHttpServer server(80);
bool cameraReady = false;
bool serverStarted = false;
CameraStreamIdleHook idleHook = nullptr;

bool streamActive = false;  // single /stream client (WebServer is one-threaded)

#if defined(CAMERA_MODEL_AI_THINKER)
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
#endif

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

void logSoftApCamLine() {
  DBG_PRINTF("[CAM] SoftAP PS=NONE ampdu_rx=%s framesize=%s quality=%d fps=%d\n",
             cameraStreamGetAmpduRx() ? "on" : "off",
             cameraStreamGetFramesizeToken(),
             cameraStreamGetQuality(),
             cameraStreamGetFpsCap());
}

/**
 * AMPDU RX is set only at wifi_init time (no esp_wifi_set_ampdu_rx_enable in this core).
 * Call after WIFI_AP mode is set and before softAP(), or after a full SoftAP restart.
 */
bool reinitWifiAmpduRx(bool enableRx) {
  esp_err_t err = esp_wifi_stop();
  if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED) {
    DBG_PRINTF("[CAM] ampdu esp_wifi_stop err=0x%x\n", err);
  }
  err = esp_wifi_deinit();
  if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT) {
    DBG_PRINTF("[CAM] ampdu esp_wifi_deinit err=0x%x\n", err);
  }

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  cfg.ampdu_rx_enable = enableRx ? 1 : 0;
  err = esp_wifi_init(&cfg);
  if (err != ESP_OK) {
    DBG_PRINTF("[CAM] ampdu esp_wifi_init err=0x%x\n", err);
    return false;
  }
  err = esp_wifi_set_mode(WIFI_MODE_AP);
  if (err != ESP_OK) {
    DBG_PRINTF("[CAM] ampdu set_mode err=0x%x\n", err);
    return false;
  }
  err = esp_wifi_start();
  if (err != ESP_OK) {
    DBG_PRINTF("[CAM] ampdu esp_wifi_start err=0x%x\n", err);
    return false;
  }
  cameraStreamCamConfigNoteAmpduRx(enableRx);
  return true;
}

bool bringUpSoftAp(bool enableAmpduRx, bool forceAmpduReinit) {
  WiFi.persistent(false);
  WiFi.disconnect(true, true);
  waitMsCoop(50);
  WiFi.mode(WIFI_OFF);
  waitMsCoop(50);
  WiFi.mode(WIFI_AP);
  waitMsCoop(50);

  // AMPDU RX is only configurable at wifi_init (no set_ampdu API in this core).
  // Boot: reinit only when disabling. Runtime ampdu_rx is applied in CameraStreamCamConfig.
  if (forceAmpduReinit || !enableAmpduRx) {
    if (!reinitWifiAmpduRx(enableAmpduRx)) {
      return false;
    }
  } else {
    cameraStreamCamConfigNoteAmpduRx(true);
  }

  IPAddress apIP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, gateway, subnet);

#if TELECON_CAM_AP_OPEN
  bool apOk = WiFi.softAP(TELECON_CAM_AP_SSID, nullptr, 1, 0, 4);
#else
  bool apOk = WiFi.softAP(TELECON_CAM_AP_SSID, TELECON_CAM_AP_PASS, 1, 0, 4);
#endif
  esp_wifi_set_ps(WIFI_PS_NONE);

  DBG_PRINTF("WiFi SoftAP %s  SSID=%s  IP=%s\n",
             apOk ? "OK" : "FAILED",
             TELECON_CAM_AP_SSID,
             WiFi.softAPIP().toString().c_str());
#if !TELECON_CAM_AP_OPEN
  DBG_PRINTF("  PASS=%s\n", TELECON_CAM_AP_PASS);
#endif
  return apOk;
}

bool initCamera() {
#if defined(CAMERA_MODEL_AI_THINKER)
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
#else
  return false;
#endif
}

bool startSoftAp() {
  const bool enableAmpduRx = (TELECON_WIFI_DISABLE_AMPDU_RX == 0);
  // Reinit Wi‑Fi only when compile-time Xiaomi workaround disables AMPDU RX.
  return bringUpSoftAp(enableAmpduRx, /*forceAmpduReinit=*/!enableAmpduRx);
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

/** Continuous multipart MJPEG for Android (preferred over polling /capture).
 *  Single client only — WebServer is one-threaded; reject if already streaming. */
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

    // Pace MJPEG when fps > 0; keep RC idle hook alive during wait (no delay()).
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
  char buf[480];
  snprintf(buf, sizeof(buf),
           "TeleCon RC Vehicle Pro — CAM Wi‑Fi Binary\n"
           "SSID: %s\n"
           "GET /stream  → MJPEG (preferred; single client)\n"
           "GET /capture → single JPEG\n"
           "GET /camconfig → GET framesize|quality|fps (Smooth/Balanced/HQ)\n"
           "TCP %d → RC:CONNECT,proto,binary + AA55/BB66 / CC11/CC22\n",
           TELECON_CAM_AP_SSID, TELECON_WIFI_CTRL_PORT);
  server.send(200, "text/plain", buf);
}

void handleStatus() {
  char buf[320];
  snprintf(buf, sizeof(buf),
           "{\"app\":\"RC\",\"proto\":\"binary\",\"camera\":%s,\"ip\":\"%s\","
           "\"framesize\":\"%s\",\"quality\":%d,\"fps\":%d,\"ampdu_rx\":%d,"
           "\"cam\":{\"framesize\":\"%s\",\"quality\":%d,\"fps_cap\":%d}}",
           cameraReady ? "true" : "false",
           WiFi.softAPIP().toString().c_str(),
           cameraStreamGetFramesizeToken(),
           cameraStreamGetQuality(),
           cameraStreamGetFpsCap(),
           cameraStreamGetAmpduRx(),
           cameraStreamGetFramesizeToken(),
           cameraStreamGetQuality(),
           cameraStreamGetFpsCap());
  server.send(200, "application/json", buf);
}

}  // namespace

void cameraStreamSetIdleHook(CameraStreamIdleHook hook) {
  idleHook = hook;
}

void cameraStreamBegin() {
  if (serverStarted) return;

  cameraStreamCamConfigInit(TELECON_CAM_DEFAULT_FRAMESIZE,
                            TELECON_CAM_DEFAULT_QUALITY,
                            TELECON_CAM_DEFAULT_FPS);
  cameraStreamCamConfigSetSoftApCredentials(
      TELECON_CAM_AP_SSID,
#if TELECON_CAM_AP_OPEN
      nullptr
#else
      TELECON_CAM_AP_PASS
#endif
  );
  startSoftAp();
  logSoftApCamLine();
  cameraReady = initCamera();
  DBG_PRINTF("Camera sensor %s\n", cameraReady ? "OK" : "FAILED");

  server.on("/", handleRoot);
  server.on("/stream", HTTP_GET, handleStream);
  server.on("/capture", HTTP_GET, handleCapture);
  server.on("/status", HTTP_GET, handleStatus);
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
