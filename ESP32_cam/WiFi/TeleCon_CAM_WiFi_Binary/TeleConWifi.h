#pragma once

/*
 * TeleConWifi.h — SoftAP TCP transport (Wi‑Fi counterpart of SerialBT / NUS).
 *
 * On CAM (TELECON_WIFI_OWN_SOFTAP 0): CameraStream owns SoftAP; this module
 * only runs WiFiServer on TELECON_WIFI_CTRL_PORT (text lines + AA/BB/CC frames).
 */

#include <Arduino.h>

bool teleconWifiBegin();
void teleconWifiPoll();
/** Serial Monitor → phone (TELECON_SERIAL_INJECT). */
void teleconWifiSerialPoll();

bool teleconWifiSendLine(const char* line);
bool teleconWifiSendBinary(const uint8_t* data, size_t len);

bool teleconWifiIsConnected();
/** True after TCP client is up (Wi‑Fi has no CCCD; always ready when connected). */
bool teleconWifiReady();

/** Called from TeleConBinaryRx when a full text line is assembled. */
void onLineReceived(const String& line);
