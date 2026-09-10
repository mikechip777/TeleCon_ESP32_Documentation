/*
 * TeleCon RC Vehicle Pro — Wi‑Fi CAM Simple (ESP32-CAM, Normal starter)
 *
 * Android (TeleCon4ESP32):
 *   1. Phone Wi‑Fi → join SoftAP "TeleCon-RC-CAM-Starter" / telecon1234
 *   2. RC Vehicle Pro → Board CAM → Normal → Wi‑Fi CAM Starter
 *   3. Connect (TCP 192.168.4.1:3333)
 *      Handshake: RC:CONNECT,proto,simple  (also accepts proto,wifi)
 *
 * SoftAP + MJPEG owned by CameraStream; this .ino owns TCP text control.
 * TELECON_VEHICLE_IO conceptually off — camera pins free; motors = YOUR CODE HERE.
 * Advanced Binary: ../TeleCon_CAM_WiFi_Binary/
 */

#include <WiFi.h>
#include "CameraStream.h"
#include "SteerCenter.h"
#include "DebugConfig.h"

// === CONFIG ===
#define AP_SSID             "TeleCon-RC-CAM-Starter"
#define AP_PASS             "telecon1234"
#define WIFI_CTRL_PORT      3333
#define APP_PREFIX          "RC"
#define PROTO_WIRE          "simple"
#define SERIAL_BAUD         115200

#define TELEMETRY_DATA_MS   500
#define CTRL_TIMEOUT_MS     750

#define DEMO_MOTOR_TEMP     90
#define DEMO_BATT_PCT       76

// === WIFI / TCP ===
WiFiServer server(WIFI_CTRL_PORT);
WiFiClient client;

bool handshakeOk = false;
bool clientWasConnected = false;
bool failSafeActive = true;
String lineBuf;

int g_lx = 0, g_ly = 0, g_rx = 0, g_ry = 0;
int g_lk = 512, g_rk = 512;
uint8_t g_sw = 0;

unsigned long lastDataMs = 0;
unsigned long lastCtrlMs = 0;

static uint16_t lastPrintLX = 0xFFFF;
static uint16_t lastPrintLY = 0xFFFF;
static uint16_t lastPrintRX = 0xFFFF;
static uint16_t lastPrintRY = 0xFFFF;
static uint16_t lastPrintKnobL = 0xFFFF;
static uint16_t lastPrintKnobR = 0xFFFF;
static uint8_t lastPrintSw = 0xFF;

String getValue(const String& line, const char* key);
void handleLine(const String& line);
void handleConnect(const String& app, const String& line);
void sendLine(const String& line);
void resetHandshake();
void applyFailSafe();
void onClientGone();
void debugPrintControl(int lx, int ly, int rx, int ry, int lk, int rk, uint8_t sw);
void debugPrintButton(int id);
uint16_t stickTo12(int s);
void tcpControlPoll();

void onControl(int lx, int ly, int rx, int ry, int lk, int rk, uint8_t sw) {
  debugPrintControl(lx, ly, rx, ry, lk, rk, sw);

  // Record steering for NVS center lock (no onboard servo — keep camera pins free).
  // Use steerCenterMapUs(rx) / steerCenterMapBias(rx) when you wire off-board drivers.
  steerCenterNoteApplied(rx);

  // YOUR CODE HERE — drive motors / servos from sticks & knobs
  //   ly = throttle (-100..100), rx = steering, rk = pan, sw bit0 = lights
  // Keep camera GPIOs free (0,5,18,19,21–27,32,34–36,39). Prefer off-board drivers.
  (void)lx; (void)ly; (void)rx; (void)ry; (void)lk; (void)rk; (void)sw;
}

void onButton(int id) {
  debugPrintButton(id);

  // YOUR CODE HERE — button actions
  if (id == 2) {
    // Horn note — pulse buzzer briefly
  } else if (id == 4) {
    // Center pan note — move camera pan servo to center
  } else if (id == STEER_CENTER_BTN_ID) {
    steerCenterSaveFromCurrentRx(g_rx);
    sendLine("RC:ACK,steer_center,1");
  }
}

static void onCameraStreamIdle() {
  tcpControlPoll();
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(300);

  steerCenterBegin();

  cameraStreamSetIdleHook(onCameraStreamIdle);
  cameraStreamBegin(AP_SSID, AP_PASS);

  server.begin();
  server.setNoDelay(true);
  lineBuf.reserve(128);

  DBG_PRINTLN();
  DBG_PRINTLN("=== TeleCon RC Vehicle Pro — Wi‑Fi CAM Simple ===");
  DBG_PRINT("SoftAP SSID: ");
  DBG_PRINTLN(AP_SSID);
  DBG_PRINT("Password:    ");
  DBG_PRINTLN(AP_PASS);
  DBG_PRINT("AP IP:       ");
  DBG_PRINTLN(WiFi.softAPIP());
  DBG_PRINTF("TCP control: %s:%d\n", WiFi.softAPIP().toString().c_str(), WIFI_CTRL_PORT);
  DBG_PRINTLN("Video: http://192.168.4.1/stream  (MJPEG)  /capture (JPEG)");
  DBG_PRINTLN("Stream quality on phone requires /camconfig (2026-08+).");
  DBG_PRINTLN("Android: RC Vehicle Pro → Board CAM → Normal → Wi‑Fi CAM Starter");
  DBG_PRINTLN("Handshake: RC:CONNECT,proto,simple  →  RC:ACK,app,RC");
  DBG_PRINTLN("  (also accepts proto,wifi for older SoftAP CONNECT)");
  DBG_PRINTLN("Examples:");
  DBG_PRINTLN("  RC:CONNECT,proto,simple");
  DBG_PRINTLN("  RC:CTRL,lx,0,ly,0,rx,0,ry,0,lk,512,rk,512,sw,00");
  DBG_PRINTLN("  RC:BTN,id,1");
  DBG_PRINTLN("  RC:SET,steer_center,1,rx,0");
  DBG_PRINTLN("Steer trim: Android Tune → ◀/▶ → Check (center in NVS).");
  DBG_PRINTLN("Edit onControl() / onButton() at YOUR CODE HERE.");
  DBG_PRINTLN("Telemetry: RC:DATA every 500 ms (speed×10 from |ly|, demo batt/temp).");
  DBG_PRINTLN("Fail-safe: TCP drop or ~750 ms without CTRL → stop (print once).");
  DBG_PRINTLN();
}

void loop() {
  steerCenterLoop();
  cameraStreamPoll();
  tcpControlPoll();
}

void tcpControlPoll() {
  if (!client || !client.connected()) {
    if (clientWasConnected) {
      onClientGone();
      clientWasConnected = false;
    }
    if (client) client.stop();

    WiFiClient incoming = server.available();
    if (incoming) {
      client = incoming;
      client.setNoDelay(true);
      resetHandshake();
      lineBuf = "";
      clientWasConnected = true;
      DBG_PRINTF("[TCP] client %s — waiting for RC:CONNECT,proto,simple\n",
                    client.remoteIP().toString().c_str());
    }
  }

  if (!client || !client.connected()) {
    return;
  }

  while (client.available()) {
    char c = (char)client.read();
    if (c == '\n') {
      lineBuf.trim();
      if (lineBuf.length() > 0) {
        handleLine(lineBuf);
      }
      lineBuf = "";
    } else if (c != '\r') {
      if (lineBuf.length() < 512) {
        lineBuf += c;
      } else {
        lineBuf = "";
      }
    }
  }

  if (!handshakeOk) return;

  if (millis() - lastCtrlMs > CTRL_TIMEOUT_MS) {
    applyFailSafe();
  }

  unsigned long now = millis();
  if (now - lastDataMs >= TELEMETRY_DATA_MS) {
    lastDataMs = now;

    int speedX10 = abs(g_ly) * 25 / 10;
    int analog = DEMO_MOTOR_TEMP;
    int batt = DEMO_BATT_PCT;

    char data[128];
    snprintf(data, sizeof(data),
      "RC:DATA,left,%d,right,0,lo,1,ro,0,lg,1,rg,0,analog,%d,batt,%d,led,%02X",
      speedX10, analog, batt, g_sw);
    sendLine(data);
  }
}

uint16_t stickTo12(int s) {
  return (uint16_t)(((constrain(s, -100, 100) + 100) * 4095) / 200);
}

void debugPrintControl(int lx, int ly, int rx, int ry, int lk, int rk, uint8_t sw) {
  uint16_t lx12 = stickTo12(lx);
  uint16_t ly12 = stickTo12(ly);
  uint16_t rx12 = stickTo12(rx);
  uint16_t ry12 = stickTo12(ry);
  uint16_t lkU = (uint16_t)constrain(lk, 0, 1023);
  uint16_t rkU = (uint16_t)constrain(rk, 0, 1023);

  if (lx12 != lastPrintLX) {
    DBG_PRINTF("L Stick X: %u    ", lx12);
    DBG_PRINTF("L Stick Y: %u\n", ly12);
    lastPrintLX = lx12;
  }
  if (ly12 != lastPrintLY) {
    DBG_PRINTF("L Stick X: %u    ", lx12);
    DBG_PRINTF("L Stick Y: %u\n", ly12);
    lastPrintLY = ly12;
  }
  if (rx12 != lastPrintRX) {
    DBG_PRINTF("R Stick X: %u    ", rx12);
    DBG_PRINTF("R Stick Y: %u\n", ry12);
    lastPrintRX = rx12;
  }
  if (ry12 != lastPrintRY) {
    DBG_PRINTF("R Stick X: %u    ", rx12);
    DBG_PRINTF("R Stick Y: %u\n", ry12);
    lastPrintRY = ry12;
  }
  if (lkU != lastPrintKnobL) {
    DBG_PRINTF("Left Knob: %u\n", lkU);
    lastPrintKnobL = lkU;
  }
  if (rkU != lastPrintKnobR) {
    DBG_PRINTF("Right Knob: %u\n", rkU);
    lastPrintKnobR = rkU;
  }
  if (sw != lastPrintSw) {
    for (int i = 0; i < 6; i++) {
      byte bit = (1 << i);
      if ((sw & bit) != (lastPrintSw & bit)) {
        DBG_PRINTF("SW%d = %d\n", i + 1, (sw & bit) ? 1 : 0);
      }
    }
    lastPrintSw = sw;
  }
}

void debugPrintButton(int id) {
  DBG_PRINTF("BTN%d : Pressed\n", id);
}

void sendLine(const String& line) {
  if (!client || !client.connected()) return;
  client.print(line);
  client.print('\n');
}

void applyFailSafe() {
  const bool entering = !failSafeActive;
  failSafeActive = true;
  g_lx = g_ly = g_rx = g_ry = 0;
  g_lk = g_rk = 512;
  g_sw = 0;
  if (entering) {
    DBG_PRINTLN("[IO] fail-safe");
  }
  // YOUR CODE HERE — stop motors on disconnect / CTRL timeout
}

void resetHandshake() {
  handshakeOk = false;
  lastPrintLX = lastPrintLY = lastPrintRX = lastPrintRY = 0xFFFF;
  lastPrintKnobL = lastPrintKnobR = 0xFFFF;
  lastPrintSw = 0xFF;
}

void onClientGone() {
  applyFailSafe();
  resetHandshake();
  lineBuf = "";
  DBG_PRINTLN("[TCP] disconnected");
}

String getValue(const String& line, const char* key) {
  int colon = line.indexOf(':');
  if (colon < 0) return "";

  int pos = line.indexOf(',', colon);
  if (pos < 0) return "";
  pos++;

  while (pos < (int)line.length()) {
    int keyEnd = line.indexOf(',', pos);
    if (keyEnd < 0) break;

    String k = line.substring(pos, keyEnd);
    int valEnd = line.indexOf(',', keyEnd + 1);
    String v = (valEnd < 0)
      ? line.substring(keyEnd + 1)
      : line.substring(keyEnd + 1, valEnd);

    if (k == key) return v;
    pos = (valEnd < 0) ? line.length() : valEnd + 1;
  }
  return "";
}

void handleConnect(const String& app, const String& line) {
  if (!app.equalsIgnoreCase(APP_PREFIX)) {
    sendLine(String("RC:NAK,reason,app_mismatch,expected,") + APP_PREFIX +
             ",actual," + app);
    handshakeOk = false;
    return;
  }

  String proto = getValue(line, "proto");
  if (proto.length() == 0) {
    sendLine(String("RC:NAK,reason,proto_mismatch,expected,") + PROTO_WIRE +
             ",actual,unknown");
    handshakeOk = false;
    return;
  }

  // Accept proto=simple (Normal). Also accept proto=wifi for SoftAP compatibility.
  if (proto.equalsIgnoreCase(PROTO_WIRE) || proto.equalsIgnoreCase("wifi")) {
    sendLine("RC:ACK,app,RC");
    handshakeOk = true;
    lastCtrlMs = millis();
    DBG_PRINTLN("[HS] ACK — session ready");
    return;
  }

  sendLine(String("RC:NAK,reason,proto_mismatch,expected,") + PROTO_WIRE +
           ",actual," + proto);
  handshakeOk = false;
}

void handleLine(const String& line) {
  int colon = line.indexOf(':');
  if (colon <= 0) return;

  String app = line.substring(0, colon);
  String rest = line.substring(colon + 1);
  int comma = rest.indexOf(',');
  String type = (comma < 0) ? rest : rest.substring(0, comma);

  if (type.equalsIgnoreCase("CONNECT")) {
    handleConnect(app, line);
    return;
  }

  if (!handshakeOk) return;
  if (!app.equalsIgnoreCase("RC")) return;

  if (type.equalsIgnoreCase("CTRL")) {
    g_lx = getValue(line, "lx").toInt();
    g_ly = getValue(line, "ly").toInt();
    g_rx = getValue(line, "rx").toInt();
    g_ry = getValue(line, "ry").toInt();
    g_lk = getValue(line, "lk").toInt();
    g_rk = getValue(line, "rk").toInt();
    String swHex = getValue(line, "sw");
    g_sw = (uint8_t)strtol(swHex.c_str(), nullptr, 16);

    lastCtrlMs = millis();
    failSafeActive = false;
    onControl(g_lx, g_ly, g_rx, g_ry, g_lk, g_rk, g_sw);
  } else if (type.equalsIgnoreCase("BTN")) {
    onButton(getValue(line, "id").toInt());
  } else if (type.equalsIgnoreCase("SET")) {
    if (steerCenterHandleSetLine(line, g_rx)) {
      sendLine("RC:ACK,steer_center,1");
    }
  }
}
