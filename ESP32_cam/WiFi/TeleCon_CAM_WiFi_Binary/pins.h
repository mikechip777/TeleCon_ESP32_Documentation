#pragma once

/*
 * pins.h — ESP32-CAM (AI-Thinker) GPIO map.
 *
 * Camera occupies: 0, 5, 18, 19, 21, 22, 23, 25, 26, 27, 32, 34, 35, 36, 39.
 * PSRAM typically uses GPIO 16 — do not drive it.
 * TELECON_VEHICLE_IO = 0 skips motor outputs (camera pins stay free).
 *
 * Bench / minimal (safe with SoftAP + camera):
 *   GPIO 33 — status LED (onboard red LED on many AI-Thinker boards)
 *   GPIO 13 — optional pan servo
 *   GPIO 14 — optional buzzer
 *   GPIO 15 — optional lights
 *
 * Full vehicle motors need off-board drivers on free pins; do not reuse camera GPIOs.
 */

#define PIN_STATUS_LED     33

#define PIN_PAN_SERVO      13
#define PIN_BUZZER         14
#define PIN_LIGHTS         15

/* Optional off-board drive (only when TELECON_VEHICLE_IO 1 and wired carefully).
 * Defaults leave motor pins unused (-1) so CAM bring-up stays safe. */
#define PIN_DRIVE_L_PWM   (-1)
#define PIN_DRIVE_L_DIR   (-1)
#define PIN_DRIVE_R_PWM   (-1)
#define PIN_DRIVE_R_DIR   (-1)
#define PIN_STEER_SERVO   (-1)

#define PIN_BATT_ADC      (-1)
#define PIN_MOTOR_TEMP_ADC (-1)

#define CH_DRIVE_L  0
#define CH_DRIVE_R  1
#define CH_STEER    2
#define CH_PAN      3
