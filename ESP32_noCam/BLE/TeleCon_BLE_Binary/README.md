# TeleCon — BLE Binary (Advanced)

Complete architecture for **Advanced** DevKit users (BLE NUS + binary RC).  
Role A video overlay: flash `ESP32_cam/WiFi/TeleCon_CAM_SoftAP_Video/` on a separate CAM.

## Board

ESP32 DevKit (no camera)

## Bluetooth

- Device name: `ESP32-TC-RC-BLE-Binary`
- App prefix: `RC`
- Android Control Panel or **RC Vehicle Pro** → Board **DevKit** → **Advanced** → **BLE Binary**

## Protocol

- Handshake: `RC:CONNECT,proto,binary` within ~2.5 s
- Success: `RC:ACK,app,RC`
- Wrong proto/app: `RC:NAK,reason,proto_mismatch|app_mismatch,...`

Binary: `AA 55` (18 B) control, `BB 66` buttons, `CC 11`/`CC 22` telemetry + optional `CC 33` debug plot (no `CC 44`).

## Control map

| UI | Wire | Action |
|----|------|--------|
| Left stick Y | `ly` | Throttle (differential drive) |
| Right stick X | `rx` | Steering / mix |
| Right knob | `rk` | Camera pan servo (center 512) |
| Lights | switch1 / `sw` bit0 | Lights GPIO |
| Buzzer | BTN 2 | Horn pulse |
| Camera front | BTN 4 | Center pan |
| E-stop / disconnect | sticks 0 / link drop | Motors off |

## Telemetry

- Speed km/h × 10 → panel `left` / `CC 11` left (`lo=1`)
- Battery % → `batt` / `CC 22`
- Motor temp → `analog` 0–255 / `CC 22`
- Optional debug plot → `CC 33` count=4 (~20 Hz; HUD ignores)

Serial inject (after handshake): `RC:DATA` / `RC:PLOT` → one-shot `CC` frames (Control Panel style).


## Wiring (DevKit — `TELECON_VEHICLE_IO=1`)

| Function | GPIO |
|----------|------|
| Drive L PWM / DIR | 25 / 26 |
| Drive R PWM / DIR | 27 / 14 |
| Steer servo | 13 |
| Camera pan servo | 12 |
| Lights | 15 |
| Buzzer | 2 |
| Status LED | 4 |
| Battery ADC | 34 |
| Motor temp ADC | 35 |

Edit `pins.h` for your motor driver.

## Camera

This board has no Wi‑Fi stream. For Role A video, flash `TeleCon_CAM_SoftAP_Video` and join `TeleCon-RC-CAM-Starter`.

## Flash

1. Board: ESP32 DevKit  
2. Open this folder’s `.ino`  
3. Upload; Serial Monitor @ 115200  
4. Pair/connect from Android as BLE Binary (`ESP32-TC-RC-BLE-Binary`)

## Steer center trim (Android)

1. Connect with the mode in this README (Classic / BLE / SoftAP x Simple or Binary).
2. On **RC Vehicle Pro**, tap the Tune icon on the steering control.
3. Keep the stick at visual zero; use **left/right tickers** until the wheels look straight.
4. Tap **Check** — phone sends `RC:SET,steer_center,1,rx,<n>` and/or `BB 66` id `0x10`.
5. Firmware stores the current steering PWM / mix bias in **NVS** on the vehicle.
6. After reboot, stick `rx = 0` (and E-stop sticks 0) hold that mechanical straight.

Module: `SteerCenter.h` / `SteerCenter.cpp` in this sketch folder.

## Fail-safe

On BT/BLE disconnect, motors are zeroed immediately via `rcVehicleFailSafe()`.
