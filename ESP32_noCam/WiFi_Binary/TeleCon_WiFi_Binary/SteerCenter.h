#pragma once

/*
 * SteerCenter — shared mechanical steering center (NVS + map).
 *
 * This file lives in the sketch folder (Arduino compiles only that folder).
 * Copied into each sketch folder so Arduino IDE compiles it.
 *
 * Android lock-center:
 *   Simple / SoftAP text: RC:SET,steer_center,1,rx,<n>
 *   Binary / BLE / SoftAP binary: BB 66 id 0x10 (+ optional text SET)
 *
 * After save, stick rx=0 produces the captured straight output.
 */

#include <Arduino.h>

#ifndef STEER_CENTER_US_DEFAULT
#define STEER_CENTER_US_DEFAULT 1500
#endif
#ifndef STEER_TRAVEL_US_DEFAULT
#define STEER_TRAVEL_US_DEFAULT 500
#endif
#ifndef STEER_BIAS_MAX_DEFAULT
#define STEER_BIAS_MAX_DEFAULT 100
#endif

/** BB 66 / RC:BTN id — save steering center (matches Android ButtonEvent.STEER_CENTER_SAVE). */
#ifndef STEER_CENTER_BTN_ID
#define STEER_CENTER_BTN_ID 0x10
#endif

/** Load persisted center/bias from NVS (or defaults). Call once from setup. */
void steerCenterBegin();

/** Non-blocking NVS flush — call from loop() / rcVehicleLoop(). */
void steerCenterLoop();

/** Map stick rx (−100…100) → servo pulse µs around persisted center. */
int steerCenterMapUs(int rxNeg100to100);

/** Map stick rx → differential mix bias around persisted bias0. */
int steerCenterMapBias(int rxNeg100to100);

/** LEDC 12-bit @ 50 Hz duty counts for a pulse width in µs. */
uint32_t steerCenterDuty12FromUs(int us);

/** Convenience: 12-bit duty from stick rx (also records last applied output). */
uint32_t steerCenterDuty12FromRx(int rxNeg100to100);

/** Record last applied servo µs / mix bias for a stick rx (call when driving outputs). */
void steerCenterNoteApplied(int rxNeg100to100);

/**
 * Capture last applied steering output (or derive from rx) and schedule NVS save.
 * Idempotent if called twice (SET + BB66) before the next CTRL.
 * After this, rx=0 maps to that same output.
 */
bool steerCenterSaveFromCurrentRx(int currentRxNeg100to100);

/**
 * Handle RC:SET,… line. Returns true if steer_center,1 was applied.
 * Unknown keys are ignored. Uses rx from the line when present, else fallbackRx.
 */
bool steerCenterHandleSetLine(const String& line, int fallbackRx);

int steerCenterGetUs();
int steerCenterGetBias0();
