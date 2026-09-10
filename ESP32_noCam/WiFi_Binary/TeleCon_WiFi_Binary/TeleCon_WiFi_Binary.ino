/*
 * TeleCon_WiFi_Binary
 * RC Vehicle Pro — SoftAP TCP + Binary RC (AA 55 / BB 66 / CC 11/22/33).
 *
 * Android: Board DevKit → Connection = Wi‑Fi SoftAP + Binary
 * Join ESP32-TC-RC-WiFi-Binary / telecon1234, Connect → 192.168.4.1:3333
 */

#include <WiFi.h>

#include "TeleConConfig.h"
#include "TeleConWifi.h"
#include "RcVehicleControl.h"
#include "RcVehicleHandlers.h"

void setup() {
  DBG_BEGIN(TELECON_SERIAL_BAUD);
  rcVehicleBegin();
  rcVehicleHandlersReset();

  if (!teleconWifiBegin()) {
    DBG_PRINTLN("Wi‑Fi SoftAP init failed!");
    while (true) { delay(1000); }
  }

  DBG_PRINTLN("TeleCon RC Vehicle Pro Wi‑Fi Binary ready.");
  DBG_PRINTLN("Android: RC Vehicle Pro → Board DevKit → Connection = Wi‑Fi SoftAP + Binary");
  DBG_PRINTF("SoftAP: %s / %s @ %s\n",
             TELECON_AP_SSID, TELECON_AP_PASS, WiFi.softAPIP().toString().c_str());
  DBG_PRINTF("TCP control: :%d\n", TELECON_WIFI_CTRL_PORT);
  DBG_PRINTLN("Protocol: RC:CONNECT,proto,binary → RC:ACK / RC:NAK");
  DBG_PRINTLN("Then AA 55 / BB 66 control + CC 11/22 telemetry (+ CC 33 debug plot).");
  DBG_PRINTLN("HUD: left=speed×10, batt=0..100, analog=motor temp 0..255");
#if TELECON_SERIAL_INJECT
  DBG_PRINTLN("Serial inject @ 115200 (after handshake, line ending = Newline):");
  DBG_PRINTLN("  RC:DATA,left,186,right,0,lo,1,ro,0,lg,1,rg,0,analog,90,batt,76,led,01");
  DBG_PRINTLN("  RC:PLOT,v0,128,v1,200,v2,64,v3,180");
  DBG_PRINTLN("  Inject is one-shot (pulse); auto telemetry resumes after.");
#endif
}

void loop() {
  teleconWifiPoll();
  teleconWifiSerialPoll();
  rcVehicleLoop();
  rcVehicleSendTelemetryIfDue();
}
