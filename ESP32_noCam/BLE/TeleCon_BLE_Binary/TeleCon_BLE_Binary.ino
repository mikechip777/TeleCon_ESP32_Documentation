/*
 * TeleCon_BLE_Binary
 * RC Vehicle Pro — BLE NUS + Binary RC (same wire as Classic Binary).
 */

#include "TeleConConfig.h"
#include "TeleConBle.h"
#include "RcVehicleControl.h"
#include "RcVehicleHandlers.h"

void setup() {
  DBG_BEGIN(TELECON_SERIAL_BAUD);
  rcVehicleBegin();
  rcVehicleHandlersReset();

  if (!teleconBleBegin()) {
    DBG_PRINTLN("BLE init failed!");
    while (true) { delay(1000); }
  }

  DBG_PRINTLN("TeleCon RC Vehicle Pro BLE Binary ready.");
  DBG_PRINTF("Bluetooth: %s (NUS)\n", TELECON_BLE_NAME);
  DBG_PRINTLN("Android: RC Vehicle Pro → Connection = BLE Binary");
  DBG_PRINTLN("Protocol: RC:CONNECT,proto,binary → RC:ACK / RC:NAK");
  DBG_PRINTLN("Then AA 55 / BB 66 control + CC 11/22 telemetry (+ CC 33 debug plot).");
  DBG_PRINTLN("HUD: left=speed×10, batt=0..100, analog=motor temp 0..255");
#if TELECON_SERIAL_INJECT
  DBG_PRINTLN("Serial inject @ 115200 (after handshake, line ending = Newline):");
  DBG_PRINTLN("  RC:DATA,left,186,right,0,lo,1,ro,0,lg,1,rg,0,analog,90,batt,76,led,01");
  DBG_PRINTLN("  RC:PLOT,v0,128,v1,200,v2,64,v3,180");
  DBG_PRINTLN("  Inject is one-shot (pulse); auto telemetry resumes after.");
#endif
}

void loop() {
  teleconBlePoll();
  teleconBleSerialPoll();
  rcVehicleLoop();
  rcVehicleSendTelemetryIfDue();
}
