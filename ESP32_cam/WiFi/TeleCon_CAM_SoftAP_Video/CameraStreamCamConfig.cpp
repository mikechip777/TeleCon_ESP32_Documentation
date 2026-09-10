#include "CameraStreamCamConfig.h"

#include <cctype>
#include <cstring>
#include <strings.h>

#include "esp_wifi.h"

#if __has_include("TeleConConfig.h")
#include "TeleConConfig.h"
#endif
#if __has_include("TeleConDebug.h")
#include "TeleConDebug.h"
#endif
#if __has_include("DebugConfig.h")
#include "DebugConfig.h"
#ifndef TELECON_DEBUG
#define TELECON_DEBUG ENABLE_DEBUG
#endif
#endif

#ifndef TELECON_DEBUG
#define TELECON_DEBUG 0
#endif

#ifndef DBG_PRINTF
#if TELECON_DEBUG
#define DBG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define DBG_PRINTF(...) ((void)0)
#endif
#endif
#ifndef DBG_PRINTLN
#if TELECON_DEBUG
#define DBG_PRINTLN(x) Serial.println(x)
#else
#define DBG_PRINTLN(x) ((void)0)
#endif
#endif

namespace {

framesize_t g_framesize = FRAMESIZE_VGA;
char g_token[12] = "vga";
int g_quality = CAMCONFIG_DEFAULT_QUALITY;
int g_fps = CAMCONFIG_DEFAULT_FPS;
bool g_ampduRx = true;
volatile bool g_streamActive = false;
volatile bool g_endStream = false;
WebServer* g_http = nullptr;
bool* g_cameraReady = nullptr;

char g_apSsid[33] = {};
char g_apPass[65] = {};
bool g_apHasPass = false;
bool g_haveAp = false;
bool g_pendingAmpduApply = false;
bool g_pendingAmpduValue = true;

const char* tokenFor(framesize_t fs) {
  switch (fs) {
    case FRAMESIZE_QQVGA:
      return "qqvga";
    case FRAMESIZE_QVGA:
      return "qvga";
    case FRAMESIZE_HVGA:
      return "hvga";
    case FRAMESIZE_VGA:
      return "vga";
    case FRAMESIZE_SVGA:
      return "svga";
    case FRAMESIZE_XGA:
      return "xga";
    case FRAMESIZE_HD:
      return "hd";
    case FRAMESIZE_UXGA:
      return "uxga";
    default:
      return "vga";
  }
}

void setToken(const char* token) {
  strncpy(g_token, token, sizeof(g_token) - 1);
  g_token[sizeof(g_token) - 1] = 0;
  for (char* p = g_token; *p; ++p) {
    *p = static_cast<char>(tolower(static_cast<unsigned char>(*p)));
  }
}

void writeOkJson(char* buf, size_t len) {
  const int ampdu = (g_pendingAmpduApply ? g_pendingAmpduValue : g_ampduRx) ? 1 : 0;
  snprintf(buf, len,
           "{\"ok\":true,\"framesize\":\"%s\",\"quality\":%d,\"fps\":%d,\"ampdu_rx\":%d}",
           g_token, g_quality, g_fps, ampdu);
}

void writeErrJson(char* buf, size_t len, const char* err) {
  snprintf(buf, len, "{\"ok\":false,\"error\":\"%s\"}", err);
}

void logCamconfigGet(const char* query) {
#if TELECON_DEBUG
  if (query && query[0]) {
    DBG_PRINTF("[CAM] GET /camconfig %s\n", query);
  } else {
    DBG_PRINTLN("[CAM] GET /camconfig");
  }
#endif
}

void logCamconfigApplied() {
#if TELECON_DEBUG
  const int ampdu = (g_pendingAmpduApply ? g_pendingAmpduValue : g_ampduRx) ? 1 : 0;
  DBG_PRINTF("[CAM] camconfig applied framesize=%s quality=%d fps=%d ampdu_rx=%d\n",
             g_token, g_quality, g_fps, ampdu);
#endif
}

void logCamconfigFail(int status, const char* err) {
#if TELECON_DEBUG
  DBG_PRINTF("[CAM] camconfig FAIL %d %s\n", status, err ? err : "");
#endif
}

void logSoftApCamLine() {
  DBG_PRINTF("[CAM] SoftAP PS=NONE ampdu_rx=%s framesize=%s quality=%d fps=%d\n",
             g_ampduRx ? "on" : "off",
             g_token, g_quality, g_fps);
}

bool parseIntStrict(const char* s, int* out) {
  if (!s || !s[0] || !out) return false;
  int n = 0;
  for (const char* p = s; *p; ++p) {
    if (*p < '0' || *p > '9') return false;
    n = n * 10 + (*p - '0');
    if (n > 1000) return false;
  }
  *out = n;
  return true;
}

void waitMsCoop(uint32_t ms) {
  const uint32_t start = millis();
  while ((millis() - start) < ms) {
    yield();
  }
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
  g_ampduRx = enableRx;
  return true;
}

bool bringUpSoftAp(bool enableAmpduRx) {
  if (!g_haveAp) return false;

  WiFi.persistent(false);
  WiFi.disconnect(true, true);
  waitMsCoop(50);
  WiFi.mode(WIFI_OFF);
  waitMsCoop(50);
  WiFi.mode(WIFI_AP);
  waitMsCoop(50);

  if (!reinitWifiAmpduRx(enableAmpduRx)) {
    return false;
  }

  IPAddress apIP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, gateway, subnet);

  bool apOk = g_apHasPass
                  ? WiFi.softAP(g_apSsid, g_apPass, 1, 0, 4)
                  : WiFi.softAP(g_apSsid, nullptr, 1, 0, 4);
  esp_wifi_set_ps(WIFI_PS_NONE);
  return apOk;
}

void flushPendingAmpduRx() {
  if (!g_pendingAmpduApply) return;
  g_pendingAmpduApply = false;
  if (!bringUpSoftAp(g_pendingAmpduValue)) {
    logCamconfigFail(503, "ampdu_rx apply failed");
    return;
  }
  logSoftApCamLine();
}

bool applySensor(framesize_t fs, int quality) {
  sensor_t* sensor = esp_camera_sensor_get();
  if (!sensor) return false;
  if (sensor->set_framesize) {
    sensor->set_framesize(sensor, fs);
  }
  if (sensor->set_quality) {
    sensor->set_quality(sensor, quality);
  }
  g_framesize = fs;
  g_quality = quality;
  setToken(tokenFor(fs));
  return true;
}

struct ParsedQuery {
  bool hasFs = false;
  bool hasQ = false;
  bool hasFps = false;
  bool hasAmpdu = false;
  char fsToken[12] = {};
  int quality = 0;
  int fps = 0;
  bool ampduRx = true;
};

enum ParseResult { PARSE_OK, PARSE_BAD_FS, PARSE_BAD_Q, PARSE_BAD_FPS, PARSE_BAD_AMPDU };

ParseResult parseQuery(const char* query, ParsedQuery* out) {
  *out = ParsedQuery();
  if (!query || !query[0]) return PARSE_OK;

  char buf[192];
  strncpy(buf, query, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = 0;
  char* space = strchr(buf, ' ');
  if (space) *space = 0;

  char* save = nullptr;
  for (char* pair = strtok_r(buf, "&", &save); pair; pair = strtok_r(nullptr, "&", &save)) {
    char* eq = strchr(pair, '=');
    if (!eq) continue;
    *eq = 0;
    const char* key = pair;
    const char* val = eq + 1;
    if (strcmp(key, "framesize") == 0) {
      out->hasFs = true;
      strncpy(out->fsToken, val, sizeof(out->fsToken) - 1);
      framesize_t tmp;
      if (!cameraStreamParseFramesize(out->fsToken, &tmp)) return PARSE_BAD_FS;
    } else if (strcmp(key, "quality") == 0) {
      out->hasQ = true;
      if (!parseIntStrict(val, &out->quality) || out->quality < 0 || out->quality > 63) {
        return PARSE_BAD_Q;
      }
    } else if (strcmp(key, "fps") == 0) {
      out->hasFps = true;
      if (!parseIntStrict(val, &out->fps)) return PARSE_BAD_FPS;
      if (out->fps != 0 && (out->fps < 5 || out->fps > 30)) return PARSE_BAD_FPS;
    } else if (strcmp(key, "ampdu_rx") == 0) {
      out->hasAmpdu = true;
      if (strcmp(val, "0") != 0 && strcmp(val, "1") != 0) return PARSE_BAD_AMPDU;
      out->ampduRx = (strcmp(val, "1") == 0);
    }
    // Unknown keys ignored (not 400).
  }
  return PARSE_OK;
}

int statusForParse(ParseResult pr, char* json, size_t jsonLen) {
  switch (pr) {
    case PARSE_BAD_FS:
      writeErrJson(json, jsonLen, "invalid framesize");
      logCamconfigFail(400, "invalid framesize");
      return 400;
    case PARSE_BAD_Q:
      writeErrJson(json, jsonLen, "invalid quality");
      logCamconfigFail(400, "invalid quality");
      return 400;
    case PARSE_BAD_FPS:
      writeErrJson(json, jsonLen, "invalid fps");
      logCamconfigFail(400, "invalid fps");
      return 400;
    case PARSE_BAD_AMPDU:
      writeErrJson(json, jsonLen, "invalid ampdu_rx");
      logCamconfigFail(400, "invalid ampdu_rx");
      return 400;
    case PARSE_OK:
    default:
      return 200;
  }
}

int applyParsed(const ParsedQuery& p, char* json, size_t jsonLen) {
  framesize_t nextFs = g_framesize;
  int nextQ = g_quality;
  int nextFps = g_fps;
  char nextToken[12];
  strncpy(nextToken, g_token, sizeof(nextToken) - 1);
  nextToken[sizeof(nextToken) - 1] = 0;

  if (p.hasFs) {
    if (!cameraStreamParseFramesize(p.fsToken, &nextFs)) {
      writeErrJson(json, jsonLen, "invalid framesize");
      logCamconfigFail(400, "invalid framesize");
      return 400;
    }
    strncpy(nextToken, p.fsToken, sizeof(nextToken) - 1);
    nextToken[sizeof(nextToken) - 1] = 0;
    for (char* c = nextToken; *c; ++c) {
      *c = static_cast<char>(tolower(static_cast<unsigned char>(*c)));
    }
  }
  if (p.hasQ) nextQ = p.quality;
  if (p.hasFps) nextFps = p.fps;

  const bool fsChanged = (nextFs != g_framesize);
  const bool sensorChanged = fsChanged || (nextQ != g_quality);
  if (sensorChanged) {
    if (!applySensor(nextFs, nextQ)) {
      writeErrJson(json, jsonLen, "camera not ready");
      logCamconfigFail(503, "camera not ready");
      return 503;
    }
    setToken(nextToken);
    if (fsChanged && g_streamActive) {
      g_endStream = true;
    }
  }
  g_fps = nextFps;
  if (p.hasAmpdu && p.ampduRx != g_ampduRx) {
    g_pendingAmpduApply = true;
    g_pendingAmpduValue = p.ampduRx;
  }
  writeOkJson(json, jsonLen);
  logCamconfigApplied();
  return 200;
}

int applyQuery(const char* query, char* json, size_t jsonLen) {
  ParsedQuery p;
  const int parseStatus = statusForParse(parseQuery(query, &p), json, jsonLen);
  if (parseStatus != 200) return parseStatus;
  if (!p.hasFs && !p.hasQ && !p.hasFps && !p.hasAmpdu) {
    writeOkJson(json, jsonLen);
    logCamconfigApplied();
    return 200;
  }
  return applyParsed(p, json, jsonLen);
}

void sendJson(WebServer& server, int status, const char* json) {
  server.send(status, "application/json", json);
}

void queryFromArgs(WebServer& server, char* out, size_t outLen) {
  out[0] = 0;
  size_t n = 0;
  const int nArgs = server.args();
  for (int i = 0; i < nArgs; ++i) {
    const int wrote = snprintf(out + n, outLen - n, "%s%s=%s",
                               n ? "&" : "",
                               server.argName(i).c_str(),
                               server.arg(i).c_str());
    if (wrote < 0) break;
    n += static_cast<size_t>(wrote);
    if (n >= outLen) {
      out[outLen - 1] = 0;
      break;
    }
  }
}

bool readLine(NetworkClient& c, char* buf, size_t bufLen, uint32_t timeoutMs) {
  size_t n = 0;
  const uint32_t start = millis();
  while (n + 1 < bufLen && (millis() - start) < timeoutMs) {
    if (!c.connected()) break;
    const int b = c.read();
    if (b < 0) {
      yield();
      continue;
    }
    if (b == '\n') break;
    if (b == '\r') continue;
    buf[n++] = static_cast<char>(b);
  }
  buf[n] = 0;
  return n > 0;
}

void skipHeaders(NetworkClient& c) {
  char line[96];
  while (readLine(c, line, sizeof(line), 80)) {
    if (line[0] == 0) break;
  }
}

void writeRawJson(NetworkClient& c, int status, const char* json) {
  const char* text = (status == 200) ? "OK" :
                     (status == 400) ? "Bad Request" :
                     (status == 405) ? "Method Not Allowed" :
                     (status == 503) ? "Service Unavailable" : "Error";
  const size_t len = strlen(json);
  char hdr[192];
  snprintf(hdr, sizeof(hdr),
           "HTTP/1.1 %d %s\r\n"
           "Content-Type: application/json\r\n"
           "Connection: close\r\n"
           "Content-Length: %u\r\n"
           "\r\n",
           status, text, static_cast<unsigned>(len));
  c.print(hdr);
  c.print(json);
}

void onCamConfigHttp() {
  if (!g_http) return;
  const bool ready = g_cameraReady && *g_cameraReady;
  cameraStreamCamConfigSend(*g_http, ready);
}

}  // namespace

bool cameraStreamParseFramesize(const char* token, framesize_t* out) {
  if (!token || !out) return false;
  if (strcasecmp(token, "qqvga") == 0) {
    *out = FRAMESIZE_QQVGA;
    return true;
  }
  if (strcasecmp(token, "qvga") == 0) {
    *out = FRAMESIZE_QVGA;
    return true;
  }
  if (strcasecmp(token, "hvga") == 0) {
    *out = FRAMESIZE_HVGA;
    return true;
  }
  if (strcasecmp(token, "vga") == 0) {
    *out = FRAMESIZE_VGA;
    return true;
  }
  if (strcasecmp(token, "svga") == 0) {
    *out = FRAMESIZE_SVGA;
    return true;
  }
  if (strcasecmp(token, "xga") == 0) {
    *out = FRAMESIZE_XGA;
    return true;
  }
  if (strcasecmp(token, "hd") == 0) {
    *out = FRAMESIZE_HD;
    return true;
  }
  if (strcasecmp(token, "uxga") == 0) {
    *out = FRAMESIZE_UXGA;
    return true;
  }
  return false;
}

void cameraStreamCamConfigInit(const char* framesizeToken, int quality, int fps) {
  framesize_t fs = FRAMESIZE_VGA;
  if (!framesizeToken || !cameraStreamParseFramesize(framesizeToken, &fs)) {
    fs = FRAMESIZE_VGA;
    framesizeToken = "vga";
  }
  if (quality < 0) quality = 0;
  if (quality > 63) quality = 63;
  if (fps != 0 && (fps < 5 || fps > 30)) fps = 0;
  g_framesize = fs;
  g_quality = quality;
  g_fps = fps;
  setToken(tokenFor(fs));
  g_endStream = false;
  g_pendingAmpduApply = false;
#if TELECON_DEBUG
  DBG_PRINTLN("[CAM] HTTP GET /camconfig  (Smooth/Balanced/HQ from phone)");
#endif
}

const char* cameraStreamGetFramesizeToken() { return g_token; }
framesize_t cameraStreamGetFramesize() { return g_framesize; }
int cameraStreamGetQuality() { return g_quality; }
int cameraStreamGetFpsCap() { return g_fps; }
int cameraStreamGetAmpduRx() { return g_ampduRx ? 1 : 0; }

void cameraStreamCamConfigSetSoftApCredentials(const char* ssid, const char* pass) {
  if (!ssid || !ssid[0]) {
    g_haveAp = false;
    return;
  }
  strncpy(g_apSsid, ssid, sizeof(g_apSsid) - 1);
  g_apSsid[sizeof(g_apSsid) - 1] = 0;
  if (pass && pass[0]) {
    strncpy(g_apPass, pass, sizeof(g_apPass) - 1);
    g_apPass[sizeof(g_apPass) - 1] = 0;
    g_apHasPass = true;
  } else {
    g_apPass[0] = 0;
    g_apHasPass = false;
  }
  g_haveAp = true;
}

void cameraStreamCamConfigNoteAmpduRx(bool enableRx) {
  g_ampduRx = enableRx;
}

uint32_t cameraStreamFpsPeriodMs() {
  if (g_fps <= 0) return 0;
  return 1000UL / static_cast<uint32_t>(g_fps);
}

void cameraStreamCamConfigNoteStreamActive(bool active) {
  g_streamActive = active;
  if (!active) g_endStream = false;
}

bool cameraStreamCamConfigConsumeEndStream() {
  if (!g_endStream) return false;
  g_endStream = false;
  return true;
}

int cameraStreamCamConfigSend(WebServer& server, bool cameraReady) {
  char json[192];
  if (server.method() != HTTP_GET) {
    server.sendHeader("Allow", "GET");
    writeErrJson(json, sizeof(json), "GET only");
    logCamconfigFail(405, "GET only");
    sendJson(server, 405, json);
    return 405;
  }
  char query[192];
  queryFromArgs(server, query, sizeof(query));
  logCamconfigGet(query);
  ParsedQuery parsed;
  const int parseStatus = statusForParse(parseQuery(query, &parsed), json, sizeof(json));
  if (parseStatus != 200) {
    sendJson(server, parseStatus, json);
    return parseStatus;
  }
  if (!cameraReady) {
    writeErrJson(json, sizeof(json), "camera not ready");
    logCamconfigFail(503, "camera not ready");
    sendJson(server, 503, json);
    return 503;
  }
  const int status = applyQuery(query, json, sizeof(json));
  sendJson(server, status, json);
  if (status == 200) flushPendingAmpduRx();
  return status;
}

void cameraStreamCamConfigAttach(WebServer& server, bool& cameraReady) {
  g_http = &server;
  g_cameraReady = &cameraReady;
  server.on("/camconfig", HTTP_ANY, onCamConfigHttp);
}

void cameraStreamCamConfigDrainPending(CameraStreamHttpServer& http, bool cameraReady) {
  if (!http.hasPendingClient()) return;
  NetworkClient extra = http.takePendingClient();
  if (!extra) return;
  extra.setTimeout(200);
  extra.setNoDelay(true);

  char req[192];
  if (!readLine(extra, req, sizeof(req), 120)) {
    extra.stop();
    return;
  }
  skipHeaders(extra);

  char json[192];
  const bool isGetCamconfig = (strncmp(req, "GET /camconfig", 14) == 0) &&
                              (req[14] == 0 || req[14] == ' ' || req[14] == '?');
  if (isGetCamconfig) {
    char qbuf[192];
    qbuf[0] = 0;
    const char* q = strchr(req, '?');
    if (q) {
      strncpy(qbuf, q + 1, sizeof(qbuf) - 1);
      qbuf[sizeof(qbuf) - 1] = 0;
      char* space = strchr(qbuf, ' ');
      if (space) *space = 0;
    }
    logCamconfigGet(qbuf);
    ParsedQuery parsed;
    const int parseStatus = statusForParse(parseQuery(qbuf, &parsed), json, sizeof(json));
    if (parseStatus != 200) {
      writeRawJson(extra, parseStatus, json);
    } else if (!cameraReady) {
      writeErrJson(json, sizeof(json), "camera not ready");
      logCamconfigFail(503, "camera not ready");
      writeRawJson(extra, 503, json);
    } else {
      const int status = applyQuery(qbuf, json, sizeof(json));
      writeRawJson(extra, status, json);
      if (status == 200) flushPendingAmpduRx();
    }
  } else if (strstr(req, " /camconfig")) {
    logCamconfigFail(405, "GET only");
    extra.print(F("HTTP/1.1 405 Method Not Allowed\r\nAllow: GET\r\n"
                  "Content-Type: application/json\r\nConnection: close\r\n"
                  "Content-Length: 31\r\n\r\n"
                  "{\"ok\":false,\"error\":\"GET only\"}"));
  } else {
    extra.print(F("HTTP/1.1 503 Service Unavailable\r\nConnection: close\r\n"
                  "Content-Length: 12\r\n\r\nstream busy"));
  }
  extra.stop();
}

bool cameraStreamCamConfigPollWhileStreaming(CameraStreamHttpServer& http,
                                             bool cameraReady,
                                             bool clientConnected) {
  cameraStreamCamConfigDrainPending(http, cameraReady);
  if (cameraStreamCamConfigConsumeEndStream()) return false;
  return clientConnected;
}
