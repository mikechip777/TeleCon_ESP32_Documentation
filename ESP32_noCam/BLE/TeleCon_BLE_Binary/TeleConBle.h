#pragma once

/*
 * TeleConBle.h — Nordic UART Service transport (BLE counterpart of SerialBT).
 *
 * Classic Binary uses SerialBT.read/write; this module uses NUS RX writes and
 * TX notifications with the same byte stream (text lines + AA/BB/CC frames).
 */

#include <Arduino.h>

bool teleconBleBegin();
void teleconBlePoll();
/** Serial Monitor → phone (TELECON_SERIAL_INJECT). */
void teleconBleSerialPoll();

bool teleconBleSendLine(const char* line);
bool teleconBleSendBinary(const uint8_t* data, size_t len);

bool teleconBleIsConnected();
bool teleconBleNotificationsEnabled();
void teleconBleSetNotificationsEnabled(bool enabled);

void teleconBleStartAdvertising();
void teleconBleStopAdvertising();

/** Called from TeleConBinaryRx when a full text line is assembled. */
void onLineReceived(const String& line);
