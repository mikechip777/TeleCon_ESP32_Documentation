#pragma once

/*
 * pins.h — RC Vehicle Pro GPIO map (ESP32 DevKit).
 * Adjust to your motor driver / servo wiring.
 * ESP32-CAM: keep camera pins free; prefer dual-board (Option A).
 */

#define PIN_DRIVE_L_PWM   25
#define PIN_DRIVE_L_DIR   26
#define PIN_DRIVE_R_PWM   27
#define PIN_DRIVE_R_DIR   14

#define PIN_STEER_SERVO   13
#define PIN_PAN_SERVO     12

#define PIN_LIGHTS        15
#define PIN_BUZZER         2
#define PIN_STATUS_LED     4

#define PIN_BATT_ADC      34
#define PIN_MOTOR_TEMP_ADC 35

// LEDC channels
#define CH_DRIVE_L  0
#define CH_DRIVE_R  1
#define CH_STEER    2
#define CH_PAN      3
