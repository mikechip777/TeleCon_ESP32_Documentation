#pragma once

#include <Arduino.h>
#include "BluetoothSerial.h"

extern BluetoothSerial SerialBT;

bool teleconBluetoothBegin();
void teleconBluetoothPoll();
void teleconSerialPoll();
void teleconSendLineResetCache();
bool teleconSendLine(const char* line);
bool teleconSendLine(const String& line);
void teleconSendLineForce(const char* line);
void teleconSendLineForce(const String& line);
bool teleconSendBinary(const uint8_t* data, size_t len);
void onLineReceived(const String& line);
