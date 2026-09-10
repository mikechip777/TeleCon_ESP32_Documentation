#pragma once

/*
 * TeleConConfig.h — RC Vehicle Pro BLE Binary (NUS)
 *
 * Android RC Vehicle Pro settings:
 *   Board DevKit → Connection = BLE Binary
 *   Board CAM → Kit B (SoftAP video + this DevKit for sticks)
 * Device: ESP32-TC-RC-BLE-Binary  |  Handshake: RC:CONNECT,proto,binary
 */

#define TELECON_DEBUG 1

#ifndef TELECON_USE_NIMBLE
#define TELECON_USE_NIMBLE 0
#endif

#ifndef TELECON_NOTIFY_CHUNK
#define TELECON_NOTIFY_CHUNK 20
#endif

#define TELECON_BLE_NAME      "ESP32-TC-RC-BLE-Binary"
#define TELECON_APP_PREFIX    "RC"
#define TELECON_PROTO_WIRE    "binary"
#define TELECON_SERIAL_BAUD   115200
#define TELECON_MAX_LINE      512
#define TELECON_LINE_RESERVE  128

#define TELECON_SERIAL_INJECT 1
#define TELECON_VEHICLE_IO   1  // 1=drive GPIOs (DevKit); 0=skip (CAM dual-board)
#define TELECON_HAS_CAMERA    0

#ifndef TELECON_AUTO_PLOT
#define TELECON_AUTO_PLOT 1
#endif

#define TELECON_CAM_AP_SSID   "TeleCon-RC-CAM"
#define TELECON_CAM_AP_PASS   "telecon1234"
#define CAMERA_MODEL_AI_THINKER

#define RC_PANEL_INDICATOR_MS  500
#define RC_PLOT_MS             50
#define RC_PLOT_MAX_SERIES     6

#define NUS_SERVICE_UUID  "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX_UUID       "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_TX_UUID       "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_REQUESTED_MTU 247
#define BLE_DEFAULT_CHUNK 20

#define TELECON_RC_HDR0         0xAA
#define TELECON_RC_HDR1         0x55
#define TELECON_RC_PACKET_SIZE  18
#define TELECON_BTN_HDR0        0xBB
#define TELECON_BTN_HDR1        0x66
#define TELECON_BTN_PACKET_SIZE 4

#define TELECON_PANEL_HDR0      0xCC
#define TELECON_PANEL_HDR1      0x11
#define TELECON_PANEL_SIZE      8

#define TELECON_INDICATOR_HDR0  0xCC
#define TELECON_INDICATOR_HDR1  0x22
#define TELECON_INDICATOR_SIZE  6

#define TELECON_PLOT_HDR0       0xCC
#define TELECON_PLOT_HDR1       0x33

#if TELECON_DEBUG || TELECON_SERIAL_INJECT
  #define DBG_BEGIN(baud)   Serial.begin(baud)
#else
  #define DBG_BEGIN(baud)   ((void)0)
#endif

#if TELECON_DEBUG
  #define DBG_PRINT(x)      Serial.print(x)
  #define DBG_PRINTLN(x)    Serial.println(x)
  #define DBG_PRINTF(...)   Serial.printf(__VA_ARGS__)
#else
  #define DBG_PRINT(x)      ((void)0)
  #define DBG_PRINTLN(x)    ((void)0)
  #define DBG_PRINTF(...)   ((void)0)
#endif
