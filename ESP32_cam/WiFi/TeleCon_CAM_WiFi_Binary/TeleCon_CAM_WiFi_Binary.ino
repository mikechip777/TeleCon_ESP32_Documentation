/*
 * TeleCon_CAM_WiFi_Binary
 *
 * ESP32-CAM (AI-Thinker): SoftAP video + Binary RC control on one board.
 *
 * SoftAP:  TeleCon-RC-CAM / telecon1234  → 192.168.4.1
 * Video:   GET http://192.168.4.1/stream  (MJPEG) or /capture (JPEG)
 * Config:  GET http://192.168.4.1/camconfig  (Smooth / Balanced / HQ)
 * Status:  GET http://192.168.4.1/status
 * Control: TCP 192.168.4.1:3333  (RC:CONNECT,proto,binary → AA55 / BB66 / CC11/22)
 *
 * Android: RC Vehicle Pro → Board CAM → Advanced → Wi‑Fi SoftAP Binary
 * SoftAP owned by CameraStream; TeleConWifi attaches TCP only (no double SoftAP).
 */

#include "TeleConConfig.h"
#include "TeleConDebug.h"
#include "TeleConWifi.h"
#include "RcVehicleControl.h"
#include "RcVehicleHandlers.h"
#include "CameraStream.h"

static void onCameraStreamIdle() {
  // Keep RC TCP alive while HTTP /stream handler is blocked on a client.
  teleconWifiPoll();
  rcVehicleLoop();
}

void setup() {
  DBG_BEGIN(TELECON_SERIAL_BAUD);
  delay(300);

  cameraStreamSetIdleHook(onCameraStreamIdle);
  rcVehicleBegin();
  rcVehicleHandlersReset();

  cameraStreamBegin();

  if (!teleconWifiBegin()) {
    DBG_PRINTLN("Wi‑Fi TCP control init failed!");
    while (true) { delay(1000); }
  }

  DBG_PRINTLN("---- TeleCon RC Vehicle Pro — CAM Wi‑Fi Binary ----");
  DBG_PRINTF("SSID %s  PASS %s\n", TELECON_CAM_AP_SSID, TELECON_CAM_AP_PASS);
  DBG_PRINTLN("Android: RC Vehicle Pro → Board CAM → Advanced → Wi‑Fi SoftAP Binary");
  DBG_PRINTLN("Join SoftAP, then:");
  DBG_PRINTLN("  Video  http://192.168.4.1/stream  (MJPEG)");
  DBG_PRINTLN("  Still  http://192.168.4.1/capture");
  DBG_PRINTLN("  Config http://192.168.4.1/camconfig  (GET; Smooth/Balanced/HQ)");
  DBG_PRINTLN("  Stream quality on phone requires firmware with /camconfig (2026-08+).");
  DBG_PRINTF("  Control TCP 192.168.4.1:%d\n", TELECON_WIFI_CTRL_PORT);
  DBG_PRINTLN("  Handshake RC:CONNECT,proto,binary → RC:ACK / RC:NAK");
  DBG_PRINTLN("  Then AA 55 / BB 66 control + CC 11/22 telemetry (+ CC 33 plot)");
  DBG_PRINTLN("TELECON_VEHICLE_IO=0 — camera pins free; edit RcVehicleControl for motors.");
#if TELECON_SERIAL_INJECT
  DBG_PRINTLN("Serial inject @ 115200 (after handshake, Newline): RC:DATA / RC:PLOT");
#endif
}

void loop() {
  cameraStreamPoll();
  teleconWifiPoll();
  teleconWifiSerialPoll();
  rcVehicleLoop();
  rcVehicleSendTelemetryIfDue();
}
