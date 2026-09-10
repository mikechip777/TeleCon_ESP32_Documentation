/*
 * RcVehicleHandlers.cpp — CONNECT / AA55 / BB66 / CC telemetry over SoftAP TCP.
 *
 * Debug Serial style + CC rates match Control Panel / RC Vehicle binary sketches.
 * HUD mapping stays vehicle-specific (speed×10 / batt% / motor temp).
 */

#include "RcVehicleHandlers.h"
#include "RcVehicleControl.h"
#include "SteerCenter.h"
#include "TeleConWifi.h"
#include "TeleConConfig.h"
#include "TeleConProtocol.h"
#include "CameraStream.h"

#include <string.h>

namespace {

bool handshakeConfirmed = false;
uint16_t lastPanelL = 0xFFFF;
uint16_t lastPanelR = 0xFFFF;
uint8_t lastAnalog = 0xFF;
uint8_t lastBatt = 0xFF;
uint8_t lastLed = 0xFF;
unsigned long lastIndicatorMs = 0;
unsigned long lastPlotMs = 0;

// Serial Monitor last-printed values (Control Panel debug.cpp style)
uint16_t lastPrintLX = 0xFFFF;
uint16_t lastPrintLY = 0xFFFF;
uint16_t lastPrintRX = 0xFFFF;
uint16_t lastPrintRY = 0xFFFF;
uint16_t lastPrintKnobL = 0xFFFF;
uint16_t lastPrintKnobR = 0xFFFF;
uint8_t lastPrintSw = 0xFF;

void debugPrintRcState(uint16_t lx12, uint16_t ly12, uint16_t rx12, uint16_t ry12,
                       uint16_t lk12, uint16_t rk12, uint8_t sw) {
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
  if (lk12 != lastPrintKnobL) {
    DBG_PRINTF("Left Knob: %u\n", lk12);
    lastPrintKnobL = lk12;
  }
  if (rk12 != lastPrintKnobR) {
    DBG_PRINTF("Right Knob: %u\n", rk12);
    lastPrintKnobR = rk12;
  }
  if (sw != lastPrintSw) {
    for (int i = 0; i < 6; i++) {
      uint8_t bit = (uint8_t)(1 << i);
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

void resetDebugPrintCaches() {
  lastPrintLX = lastPrintLY = lastPrintRX = lastPrintRY = 0xFFFF;
  lastPrintKnobL = lastPrintKnobR = 0xFFFF;
  lastPrintSw = 0xFF;
}

int unpack12BitLE(const uint8_t* p) {
  return (p[0] & 0xFF) | ((p[1] & 0x0F) << 8);
}

int stickFrom12Bit(int v12) {
  return (v12 * 200) / 4095 - 100;
}

int knobFrom12Bit(int v12) {
  if (v12 < 0) return 0;
  if (v12 > 1023) return 1023;
  return v12;
}

bool validateRcChecksum(const uint8_t* pkt) {
  uint16_t sum = 0;
  for (int i = 2; i <= 14; i++) sum += pkt[i];
  return (uint8_t)(sum & 0xFF) == pkt[15];
}

uint8_t checksumBody(const uint8_t* pkt, size_t size) {
  uint8_t c = 0;
  for (size_t i = 2; i + 1 < size; i++) c = (uint8_t)(c + pkt[i]);
  return c;
}

void sendRcAck() {
  teleconWifiSendLine("RC:ACK,app,RC");
  handshakeConfirmed = true;
  lastPanelL = 0xFFFF;
  lastPanelR = 0xFFFF;
  lastAnalog = 0xFF;
  lastBatt = 0xFF;
  lastLed = 0xFF;
  DBG_PRINTLN("[HS] ACK sent");
}

void sendRcNak(const char* reason, const char* expected, const char* actual) {
  char buf[128];
  snprintf(buf, sizeof(buf),
    "RC:NAK,reason,%s,expected,%s,actual,%s",
    reason, expected, actual);
  teleconWifiSendLine(buf);
  handshakeConfirmed = false;
  DBG_PRINTF("[HS] NAK sent (%s)\n", reason);
}

void applyCtrl(int lx, int ly, int rx, int ry, int lk, int rk, uint8_t sw,
               uint16_t lx12, uint16_t ly12, uint16_t rx12, uint16_t ry12,
               uint16_t lk12, uint16_t rk12) {
  RcVehicleState st;
  st.lx = lx; st.ly = ly; st.rx = rx; st.ry = ry;
  st.lk = lk; st.rk = rk; st.sw = sw;
  st.lx12 = lx12; st.ly12 = ly12; st.rx12 = rx12; st.ry12 = ry12;
  st.lk12 = lk12; st.rk12 = rk12;
  rcVehicleApply(st);
}

bool sendPanelPacket(uint16_t left, uint16_t right, uint8_t flags) {
  uint8_t pkt[TELECON_PANEL_SIZE];
  pkt[0] = TELECON_PANEL_HDR0;
  pkt[1] = TELECON_PANEL_HDR1;
  pkt[2] = left & 0xFF;
  pkt[3] = (left >> 8) & 0xFF;
  pkt[4] = right & 0xFF;
  pkt[5] = (right >> 8) & 0xFF;
  pkt[6] = flags;
  pkt[7] = checksumBody(pkt, TELECON_PANEL_SIZE);
  return teleconWifiSendBinary(pkt, TELECON_PANEL_SIZE);
}

bool sendIndicatorPacket(uint8_t analog, uint8_t batt, uint8_t led) {
  uint8_t pkt[TELECON_INDICATOR_SIZE];
  pkt[0] = TELECON_INDICATOR_HDR0;
  pkt[1] = TELECON_INDICATOR_HDR1;
  pkt[2] = analog;
  pkt[3] = batt;
  pkt[4] = led;
  pkt[5] = checksumBody(pkt, TELECON_INDICATOR_SIZE);
  return teleconWifiSendBinary(pkt, TELECON_INDICATOR_SIZE);
}

bool sendPlotPacket(uint8_t v0, uint8_t v1, uint8_t v2, uint8_t v3) {
  // CC 33 | len u16 LE | count=4 | b0..b3 | checksum
  uint8_t buf[16];
  int idx = 0;
  buf[idx++] = TELECON_PLOT_HDR0;
  buf[idx++] = TELECON_PLOT_HDR1;
  int lengthIndex = idx;
  buf[idx++] = 0;
  buf[idx++] = 0;
  buf[idx++] = 4;
  buf[idx++] = v0;
  buf[idx++] = v1;
  buf[idx++] = v2;
  buf[idx++] = v3;
  uint16_t payloadLength = (uint16_t)(idx - 4);
  buf[lengthIndex] = payloadLength & 0xFF;
  buf[lengthIndex + 1] = (payloadLength >> 8) & 0xFF;
  uint8_t cs = 0;
  for (int i = 2; i < idx; i++) cs = (uint8_t)(cs + buf[i]);
  buf[idx++] = cs;
  return teleconWifiSendBinary(buf, (size_t)idx);
}

uint8_t readPlot0() {
  return rcVehicleMotorTempAnalog();
}

uint8_t readPlot1() {
  return (uint8_t)map(constrain(abs(rcVehicleState().ly), 0, 100), 0, 100, 0, 255);
}

uint8_t readPlot2() {
  return (uint8_t)map(constrain(rcVehicleBatteryPercent(), 0, 100), 0, 100, 0, 255);
}

uint8_t readPlot3() {
  return (uint8_t)map(constrain(abs(rcVehicleState().rx), 0, 100), 0, 100, 0, 255);
}

bool canSendTelemetry() {
  return handshakeConfirmed && teleconWifiIsConnected() && teleconWifiReady();
}

}  // namespace

void rcVehicleHandlersReset() {
  lastPanelL = 0xFFFF;
  lastPanelR = 0xFFFF;
  lastAnalog = 0xFF;
  lastBatt = 0xFF;
  lastLed = 0xFF;
  lastIndicatorMs = 0;
  lastPlotMs = 0;
  resetDebugPrintCaches();
}

void rcVehicleHandshakeReset() {
  handshakeConfirmed = false;
  rcVehicleFailSafe();
}

bool rcVehicleHandshakeConfirmed() {
  return handshakeConfirmed;
}

void handleRcConnect(const String& app, const String& line) {
  if (!app.equalsIgnoreCase(TELECON_APP_PREFIX)) {
    sendRcNak("app_mismatch", TELECON_APP_PREFIX, app.c_str());
    return;
  }
  String proto = getValue(line, "proto");
  if (proto.length() == 0) {
    sendRcNak("proto_mismatch", TELECON_PROTO_WIRE, "unknown");
    return;
  }
  if (proto.equalsIgnoreCase(TELECON_PROTO_WIRE)) {
    sendRcAck();
    return;
  }
  sendRcNak("proto_mismatch", TELECON_PROTO_WIRE, proto.c_str());
}

void handleRcBinaryPacket(const uint8_t* pkt, size_t len) {
  if (pkt == nullptr || len < TELECON_RC_PACKET_SIZE) return;
  if (pkt[0] != TELECON_RC_HDR0 || pkt[1] != TELECON_RC_HDR1) return;
  if (pkt[16] != 0x0D || pkt[17] != 0x0A) return;
  if (!validateRcChecksum(pkt)) {
    DBG_PRINTLN("[RX] binary RC checksum fail");
    return;
  }

  uint16_t lx12 = (uint16_t)unpack12BitLE(pkt + 2);
  uint16_t ly12 = (uint16_t)unpack12BitLE(pkt + 4);
  uint16_t rx12 = (uint16_t)unpack12BitLE(pkt + 6);
  uint16_t ry12 = (uint16_t)unpack12BitLE(pkt + 8);
  uint16_t lk12 = (uint16_t)unpack12BitLE(pkt + 10);
  uint16_t rk12 = (uint16_t)unpack12BitLE(pkt + 12);
  uint8_t sw = pkt[14];

  debugPrintRcState(lx12, ly12, rx12, ry12, lk12, rk12, sw);
  applyCtrl(stickFrom12Bit(lx12), stickFrom12Bit(ly12),
            stickFrom12Bit(rx12), stickFrom12Bit(ry12),
            knobFrom12Bit(lk12), knobFrom12Bit(rk12), sw,
            lx12, ly12, rx12, ry12, lk12, rk12);
}

void handleRcBtnBinary(const uint8_t* pkt, size_t len) {
  if (pkt == nullptr || len < TELECON_BTN_PACKET_SIZE) return;
  if (pkt[0] != TELECON_BTN_HDR0 || pkt[1] != TELECON_BTN_HDR1) return;
  int id = pkt[2];
  debugPrintButton(id);
  rcVehicleHandleBtn(id);
}

void handleRcCtrlText(const String& line) {
  int lx = getValue(line, "lx").toInt();
  int ly = getValue(line, "ly").toInt();
  int rx = getValue(line, "rx").toInt();
  int ry = getValue(line, "ry").toInt();
  int lk = getValue(line, "lk").toInt();
  int rk = getValue(line, "rk").toInt();
  String swHex = getValue(line, "sw");
  uint8_t sw = (uint8_t)strtol(swHex.c_str(), nullptr, 16);
  auto stickTo12 = [](int s) -> uint16_t {
    return (uint16_t)(((s + 100) * 4095) / 200);
  };
  uint16_t lx12 = stickTo12(lx);
  uint16_t ly12 = stickTo12(ly);
  uint16_t rx12 = stickTo12(rx);
  uint16_t ry12 = stickTo12(ry);
  uint16_t lk12 = (uint16_t)constrain(lk, 0, 1023);
  uint16_t rk12 = (uint16_t)constrain(rk, 0, 1023);
  debugPrintRcState(lx12, ly12, rx12, ry12, lk12, rk12, sw);
  applyCtrl(lx, ly, rx, ry, lk, rk, sw, lx12, ly12, rx12, ry12, lk12, rk12);
}

void handleRcBtnText(const String& line) {
  int id = getValue(line, "id").toInt();
  debugPrintButton(id);
  rcVehicleHandleBtn(id);
}

void handleRcSetText(const String& line) {
  if (!steerCenterHandleSetLine(line, rcVehicleState().rx)) {
    return;  // unknown / ignored SET keys
  }
  rcVehicleHoldSteerCenter();
  teleconWifiSendLine("RC:ACK,steer_center,1");
#if TELECON_DEBUG
  DBG_PRINTF("[IO] steer center SET us=%d bias0=%d\n",
             steerCenterGetUs(), steerCenterGetBias0());
#endif
}

void rcVehicleSendTelemetryIfDue() {
  if (!canSendTelemetry()) return;
  unsigned long now = millis();

  if (now - lastIndicatorMs >= RC_PANEL_INDICATOR_MS) {
    lastIndicatorMs = now;

    uint16_t left = rcVehicleSpeedX10();
    uint16_t right = 0;
    if (left != lastPanelL || right != lastPanelR) {
      lastPanelL = left;
      lastPanelR = right;
      sendPanelPacket(left, right, 0x05);  // lo=1, lg=1
    }

    uint8_t analog = rcVehicleMotorTempAnalog();
    uint8_t batt = rcVehicleBatteryPercent();
    uint8_t led = rcVehicleLedMask();
    lastAnalog = analog;
    lastBatt = batt;
    lastLed = led;
    sendIndicatorPacket(analog, batt, led);
  }

#if TELECON_AUTO_PLOT
  const unsigned long plotMs =
      cameraStreamIsActive() ? RC_PLOT_MS_WHILE_STREAM : RC_PLOT_MS;
  if (now - lastPlotMs >= plotMs) {
    lastPlotMs = now;
    sendPlotPacket(readPlot0(), readPlot1(), readPlot2(), readPlot3());
  }
#endif
}

bool rcVehicleHandleSerialInjectLine(const String& line) {
  if (!teleconWifiIsConnected()) {
    DBG_PRINTLN("[SERIAL] no TCP client — line not sent");
    return false;
  }
  if (!handshakeConfirmed) {
    DBG_PRINTLN("[SERIAL] waiting for RC:CONNECT handshake before inject");
    return false;
  }

  if (line.startsWith("RC:DATA")) {
    uint16_t left = (uint16_t)constrain(getValue(line, "left").toInt(), 0, 9999);
    uint16_t right = (uint16_t)constrain(getValue(line, "right").toInt(), 0, 9999);
    bool lo = getValue(line, "lo").toInt() != 0;
    bool ro = getValue(line, "ro").toInt() != 0;
    bool lg = getValue(line, "lg").toInt() != 0;
    bool rg = getValue(line, "rg").toInt() != 0;
    uint8_t analog = (uint8_t)constrain(getValue(line, "analog").toInt(), 0, 255);
    uint8_t batt = (uint8_t)constrain(getValue(line, "batt").toInt(), 0, 255);
    String ledHex = getValue(line, "led");
    uint8_t led = (uint8_t)strtol(ledHex.c_str(), nullptr, 16);

    uint8_t flags = 0;
    if (lo) flags |= (1 << 0);
    if (ro) flags |= (1 << 1);
    if (lg) flags |= (1 << 2);
    if (rg) flags |= (1 << 3);

    lastPanelL = 0xFFFF;
    lastPanelR = 0xFFFF;
    bool ok = sendPanelPacket(left, right, flags);
    ok = sendIndicatorPacket(analog, batt, led) && ok;
    DBG_PRINTLN("[SERIAL] inject RC:DATA → CC 11 + CC 22 (one-shot)");
    return ok;
  }

  if (line.startsWith("RC:PLOT")) {
    uint8_t v0 = (uint8_t)constrain(getValue(line, "v0").toInt(), 0, 255);
    uint8_t v1 = (uint8_t)constrain(getValue(line, "v1").toInt(), 0, 255);
    uint8_t v2 = (uint8_t)constrain(getValue(line, "v2").toInt(), 0, 255);
    uint8_t v3 = (uint8_t)constrain(getValue(line, "v3").toInt(), 0, 255);
    bool ok = sendPlotPacket(v0, v1, v2, v3);
    DBG_PRINTLN("[SERIAL] inject RC:PLOT → CC 33 (one-shot)");
    return ok;
  }

  DBG_PRINT("[SERIAL] unhandled inject (use RC:DATA / RC:PLOT): ");
  DBG_PRINTLN(line);
  return false;
}
