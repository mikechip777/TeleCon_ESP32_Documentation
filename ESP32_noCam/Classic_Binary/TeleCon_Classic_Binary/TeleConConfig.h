#pragma once

/*
 * TeleConConfig.h — RC Vehicle Pro Classic Binary (SPP)
 *
 * Android RC Vehicle Pro settings (Board = DevKit):
 *   Connection type = Classic + Binary
 * Device: ESP32-TC-RC-BT-Binary  |  Handshake: RC:CONNECT,proto,binary
 *
 * Labels / stick modes live in the Android Control Panel settings screen only;
 * this vehicle firmware never receives label packets (no CC 44 / RC:PLOTCFG).
 */

#define TELECON_DEBUG 1

#define TELECON_BT_NAME     "ESP32-TC-RC-BT-Binary"
#define TELECON_APP_PREFIX  "RC"
#define TELECON_PROTO_WIRE  "binary"
#define TELECON_SERIAL_BAUD 115200
#define TELECON_MAX_LINE    512
#define TELECON_LINE_RESERVE 128

// 1 = Serial Monitor RC:DATA / RC:PLOT → one-shot CC frames (Control Panel style)
#define TELECON_SERIAL_INJECT 1
#define TELECON_VEHICLE_IO   1  // 1=drive GPIOs (DevKit); 0=skip (CAM dual-board)
#define TELECON_HAS_CAMERA    0

// Optional debug plot stream (Android HUD ignores plots; Control Panel can show them)
#ifndef TELECON_AUTO_PLOT
#define TELECON_AUTO_PLOT 1
#endif

#define TELECON_CAM_AP_SSID   "TeleCon-RC-CAM"
#define TELECON_CAM_AP_PASS   "telecon1234"
#define CAMERA_MODEL_AI_THINKER

#define RC_PANEL_INDICATOR_MS  500   // CC 11 / CC 22
#define RC_PLOT_MS             50    // CC 33 ~20 Hz
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
