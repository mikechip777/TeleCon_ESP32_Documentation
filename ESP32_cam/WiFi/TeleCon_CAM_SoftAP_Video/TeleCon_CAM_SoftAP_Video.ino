/*
 * TeleCon_CAM_SoftAP_Video — Role A overlay (ESP32-CAM)
 *
 * Shared by Control Panel and RC Vehicle Pro.
 * Use this when a separate DevKit handles control over Bluetooth
 * (Classic Simple, Classic Binary, or BLE Binary). This board only
 * serves SoftAP HTTP video — no TCP :3333, no Classic/BLE on the CAM.
 *
 * SoftAP:  TeleCon-RC-CAM-Starter / telecon1234  → 192.168.4.1
 * Video:   GET http://192.168.4.1/stream  (MJPEG) or /capture (JPEG)
 * Config:  GET http://192.168.4.1/camconfig
 *
 * Pair with any shared DevKit sketch:
 *   TeleCon_ESP32/.../TeleCon_Classic_Simple/  (ESP32-TC-RC-BT-Simple)
 *   TeleCon_ESP32/.../TeleCon_Classic_Binary/  (ESP32-TC-RC-BT-Binary)
 *   TeleCon_ESP32/.../TeleCon_BLE_Binary/      (ESP32-TC-RC-BLE-Binary)
 *
 * Single-board SoftAP video + TCP (RC Vehicle Pro only):
 *   TeleCon_ESP32/.../TeleCon_CAM_WiFi_Simple/
 */

#include <Arduino.h>
#include "CameraStream.h"
#include "DebugConfig.h"

#define AP_SSID      "TeleCon-RC-CAM-Starter"
#define AP_PASS      "telecon1234"
#define SERIAL_BAUD  115200

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(300);

  cameraStreamBegin(AP_SSID, AP_PASS);

  Serial.println();
  Serial.println("==== TeleCon — SoftAP video only ====");
  Serial.print("SoftAP SSID: ");
  Serial.println(AP_SSID);
  Serial.print("Password:    ");
  Serial.println(AP_PASS);
  Serial.println("Join SoftAP, then:");
  Serial.println("  Video  http://192.168.4.1/stream  (MJPEG)");
  Serial.println("  Still  http://192.168.4.1/capture");
  Serial.println("  Config http://192.168.4.1/camconfig  (Smooth/Balanced/HQ)");
  Serial.println("No TCP :3333 / no Bluetooth on this CAM.");
  Serial.println("Control: flash a DevKit Classic Simple, Classic Binary, or BLE Binary sketch.");
  Serial.println("Single-board SoftAP control+video: use TeleCon_CAM_WiFi_Simple.");
  Serial.println();
}

void loop() {
  cameraStreamPoll();
}
