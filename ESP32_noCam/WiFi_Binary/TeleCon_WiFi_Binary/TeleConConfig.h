#pragma once

/*
 * TeleConConfig.h — RC Vehicle Pro Wi‑Fi Binary (SoftAP TCP)
 *
 * Same RC binary protocol as Classic/BLE Binary; transport is SoftAP TCP :3333.
 *
 * Android RC Vehicle Pro settings (Board = DevKit):
 *   Connection type = Wi‑Fi SoftAP + Binary
 * Join SoftAP ESP32-TC-RC-WiFi-Binary / telecon1234, then Connect (192.168.4.1:3333).
 * Handshake: RC:CONNECT,proto,binary
 * No camera / HTTP on this noCam board.
 */

#define TELECON_DEBUG 1

#define TELECON_AP_SSID        "ESP32-TC-RC-WiFi-Binary"
#define TELECON_AP_PASS        "telecon1234"
#define TELECON_WIFI_CTRL_PORT 3333

#define TELECON_APP_PREFIX  "RC"
#define TELECON_PROTO_WIRE  "binary"
#define TELECON_SERIAL_BAUD 115200
#define TELECON_MAX_LINE    512
#define TELECON_LINE_RESERVE 128

#define TELECON_SERIAL_INJECT 1
#define TELECON_VEHICLE_IO   1
#define TELECON_HAS_CAMERA    0

#ifndef TELECON_AUTO_PLOT
#define TELECON_AUTO_PLOT 1
#endif

#define RC_PANEL_INDICATOR_MS  500
#define RC_PLOT_MS             50
#define RC_PLOT_MAX_SERIES     6

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
