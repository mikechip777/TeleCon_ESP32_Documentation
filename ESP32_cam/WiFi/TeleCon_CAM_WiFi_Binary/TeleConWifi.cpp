/*
 * TeleConWifi.cpp — SoftAP TCP :3333: accept client, RX → BinaryRx, TX lines/binary.
 *
 * On CAM boards (TELECON_WIFI_OWN_SOFTAP 0), CameraStream already brought SoftAP up;
 * this module only starts the TCP control server on the same AP.
 */

#include "TeleConWifi.h"
#include "TeleConBinaryRx.h"
#include "TeleConConfig.h"
#include "TeleConProtocol.h"
#include "RcVehicleHandlers.h"
#include "pins.h"

#include <WiFi.h>
#include <string.h>

namespace {

WiFiServer server(TELECON_WIFI_CTRL_PORT);
WiFiClient client;
bool clientWasConnected = false;
String lastRxLogLine;

#if TELECON_SERIAL_INJECT
String serialLineBuffer;
#endif

void onTcpClientChanged(bool connected) {
  rcVehicleHandshakeReset();
  teleconBinaryRxReset();
  if (connected) {
    rcVehicleHandlersReset();
    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, HIGH);
    DBG_PRINTLN("[TCP] client connected — waiting for RC:CONNECT,proto,binary");
  } else {
    digitalWrite(PIN_STATUS_LED, LOW);
    DBG_PRINTLN("[TCP] client disconnected");
  }
}

}  // namespace

void onLineReceived(const String& line) {
  String app, type;
  if (!parseLine(line, app, type)) {
    if (line != lastRxLogLine) {
      lastRxLogLine = line;
      DBG_PRINTLN("[RX] malformed line");
      DBG_PRINTLN(line);
    }
    return;
  }

  if (line != lastRxLogLine) {
    lastRxLogLine = line;
    DBG_PRINT("[RX] ");
    DBG_PRINTLN(line);
  }

  if (type == "CONNECT") {
    handleRcConnect(app, line);
    return;
  }

  // Prefer binary; accept text CTRL/BTN for Serial inject / benches
  if (app == TELECON_APP_PREFIX && type == "CTRL") {
    handleRcCtrlText(line);
    return;
  }
  if (app == TELECON_APP_PREFIX && type == "BTN") {
    handleRcBtnText(line);
    return;
  }
  if (app == TELECON_APP_PREFIX && type == "SET") {
    handleRcSetText(line);
    return;
  }

  DBG_PRINT("[RX] unhandled: ");
  DBG_PRINTLN(line);
}

bool teleconWifiBegin() {
#if TELECON_WIFI_OWN_SOFTAP
  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(TELECON_AP_SSID, TELECON_AP_PASS)) {
    DBG_PRINTLN("[WIFI] SoftAP failed");
    return false;
  }
#else
  // SoftAP already up via CameraStream — attach TCP only.
  if (WiFi.softAPIP() == IPAddress(0, 0, 0, 0)) {
    DBG_PRINTLN("[WIFI] SoftAP not up (CameraStream must begin first)");
    return false;
  }
#endif

  server.begin();
  server.setNoDelay(true);
  lastRxLogLine = "";
  clientWasConnected = false;

  pinMode(PIN_STATUS_LED, OUTPUT);
  digitalWrite(PIN_STATUS_LED, LOW);

  DBG_PRINTF("[WIFI] TCP control :%d on SoftAP %s IP=%s\n",
             TELECON_WIFI_CTRL_PORT,
             TELECON_AP_SSID,
             WiFi.softAPIP().toString().c_str());
  return true;
}

void teleconWifiPoll() {
  if (!client || !client.connected()) {
    if (clientWasConnected) {
      onTcpClientChanged(false);
      clientWasConnected = false;
      lastRxLogLine = "";
    }
    if (client) client.stop();

    WiFiClient incoming = server.available();
    if (incoming) {
      client = incoming;
      client.setNoDelay(true);
      clientWasConnected = true;
      lastRxLogLine = "";
      onTcpClientChanged(true);
    }
    return;
  }

  while (client.available()) {
    uint8_t b = (uint8_t)client.read();
    teleconBinaryRxFeed(b);
  }
}

void teleconWifiSerialPoll() {
#if TELECON_SERIAL_INJECT
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n') {
      if (serialLineBuffer.length() > 0) {
        serialLineBuffer.trim();
        if (serialLineBuffer.length() > 0) {
          rcVehicleHandleSerialInjectLine(serialLineBuffer);
        }
        serialLineBuffer = "";
      }
    } else if (c != '\r') {
      if (serialLineBuffer.length() < TELECON_MAX_LINE) {
        serialLineBuffer += c;
      } else {
        serialLineBuffer = "";
        DBG_PRINTLN("[SERIAL] line too long, discarded");
      }
    }
  }
#endif
}

bool teleconWifiSendLine(const char* line) {
  if (!teleconWifiIsConnected() || line == nullptr) return false;
  client.print(line);
  client.print('\n');
  return true;
}

bool teleconWifiSendBinary(const uint8_t* data, size_t len) {
  if (!teleconWifiIsConnected() || data == nullptr || len == 0) return false;
  size_t n = client.write(data, len);
  return n == len;
}

bool teleconWifiIsConnected() {
  return client && client.connected();
}

bool teleconWifiReady() {
  return teleconWifiIsConnected();
}
