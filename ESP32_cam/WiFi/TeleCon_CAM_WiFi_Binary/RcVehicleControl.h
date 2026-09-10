#pragma once

#include <Arduino.h>
#include <stdint.h>

struct RcVehicleState {
  int lx = 0;       // -100..100 (unused on vehicle HUD)
  int ly = 0;       // throttle
  int rx = 0;       // steering
  int ry = 0;
  int lk = 512;
  int rk = 512;     // camera pan knob (center ~512)
  uint8_t sw = 0;   // bit0 = lights
  uint16_t lx12 = 2048;
  uint16_t ly12 = 2048;
  uint16_t rx12 = 2048;
  uint16_t ry12 = 2048;
  uint16_t lk12 = 512;
  uint16_t rk12 = 512;
};

void rcVehicleBegin();
void rcVehicleApply(const RcVehicleState& state);
void rcVehicleHandleBtn(int id);
/** Persist current steer output as mechanical center (NVS). Returns true if scheduled. */
bool rcVehicleSaveSteerCenter();
/** Hold servo/mix at saved straight without writing NVS again. */
void rcVehicleHoldSteerCenter();
void rcVehicleFailSafe();
void rcVehicleLoop();  // non-blocking buzzer / NVS flush / outputs refresh

const RcVehicleState& rcVehicleState();

/** Speed for HUD as km/h × 10 (186 → 18.6). */
uint16_t rcVehicleSpeedX10();
/** Battery 0–100 %. */
uint8_t rcVehicleBatteryPercent();
/** Motor temp gauge 0–255. */
uint8_t rcVehicleMotorTempAnalog();
uint8_t rcVehicleLedMask();
