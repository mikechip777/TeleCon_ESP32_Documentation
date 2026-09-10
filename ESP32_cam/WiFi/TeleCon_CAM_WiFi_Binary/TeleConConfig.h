#pragma once

/*
 * TeleConConfig.h — RC Vehicle Pro CAM Wi‑Fi SoftAP Binary
 *
 * SoftAP video (HTTP) + binary RC control (TCP :3333) on one ESP32-CAM.
 * SoftAP is owned by CameraStream; TeleConWifi runs TCP only on the same AP.
 *
 * Android RC Vehicle Pro settings (Board = CAM):
 *   Connection type = Wi‑Fi SoftAP + Binary  (not legacy text proto=wifi)
 * Join TeleCon-RC-CAM / telecon1234, then Connect (192.168.4.1:3333).
 * Handshake: RC:CONNECT,proto,binary
 */

#define TELECON_DEBUG 1

#define TELECON_AP_SSID        "TeleCon-RC-CAM"
#define TELECON_AP_PASS        "telecon1234"
#define TELECON_CAM_AP_SSID    TELECON_AP_SSID
#define TELECON_CAM_AP_PASS    TELECON_AP_PASS
#ifndef TELECON_CAM_AP_OPEN
#define TELECON_CAM_AP_OPEN    0
#endif

#define TELECON_WIFI_CTRL_PORT 3333

/** CameraStream owns SoftAP; TeleConWifi begins TCP server only. */
#define TELECON_WIFI_OWN_SOFTAP 0

#define TELECON_APP_PREFIX  "RC"
#define TELECON_PROTO_WIRE  "binary"
#define TELECON_SERIAL_BAUD 115200
#define TELECON_MAX_LINE    512
#define TELECON_LINE_RESERVE 128

#define TELECON_SERIAL_INJECT 1
#define TELECON_VEHICLE_IO    0   // ESP32-CAM: keep camera pins free
#define TELECON_HAS_CAMERA    1

#ifndef TELECON_AUTO_PLOT
#define TELECON_AUTO_PLOT 1
#endif

#define RC_PANEL_INDICATOR_MS  500
#define RC_PLOT_MS             50
/** While MJPEG /stream is open, plots share SoftAP with video — keep this slow. */
#define RC_PLOT_MS_WHILE_STREAM 200
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

// SoftAP camera defaults — Android RC Vehicle Smooth (overridable via GET /camconfig)
#ifndef TELECON_CAM_DEFAULT_FRAMESIZE
#define TELECON_CAM_DEFAULT_FRAMESIZE "qqvga"
#endif
#ifndef TELECON_CAM_DEFAULT_QUALITY
#define TELECON_CAM_DEFAULT_QUALITY 28
#endif
#ifndef TELECON_CAM_DEFAULT_FPS
#define TELECON_CAM_DEFAULT_FPS     8
#endif

#define TELECON_CAM_SMOOTH_FRAMESIZE "qqvga"
#define TELECON_CAM_SMOOTH_QUALITY   28
#define TELECON_CAM_SMOOTH_FPS       8

#ifndef TELECON_WIFI_DISABLE_AMPDU_RX
#define TELECON_WIFI_DISABLE_AMPDU_RX 0
#endif

#define CAMERA_MODEL_AI_THINKER

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
