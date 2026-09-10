#pragma once

#include <Arduino.h>

void rcVehicleHandlersReset();
void rcVehicleHandshakeReset();
bool rcVehicleHandshakeConfirmed();

void handleRcConnect(const String& app, const String& line);
void handleRcBinaryPacket(const uint8_t* pkt, size_t len);
void handleRcBtnBinary(const uint8_t* pkt, size_t len);
void handleRcCtrlText(const String& line);
void handleRcBtnText(const String& line);
void handleRcSetText(const String& line);

void rcVehicleSendTelemetryIfDue();
/** Serial Monitor → encode RC:DATA / RC:PLOT to CC 11/22/33 (one-shot). */
bool rcVehicleHandleSerialInjectLine(const String& line);
