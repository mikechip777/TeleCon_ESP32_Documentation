# TeleCon — Classic Simple (Normal)

Shared Bluetooth Classic + SIMPLE text starter for ESP32 DevKit (no camera).

## Flash

1. Open `TeleCon_Classic_Simple.ino` in Arduino IDE.
2. Board: **ESP32 Dev Module**.
3. Upload, open Serial Monitor at **115200**.

## Android

1. Pair Bluetooth **`ESP32-TC-RC-BT-Simple`** (PIN **1234** if asked).
2. Control Panel or **RC Vehicle Pro** → Board **DevKit** → **Normal** → **Classic + Simple**.
3. Connect — handshake `RC:CONNECT,proto,simple` → `RC:ACK,app,RC`.

## What to edit

Put motor / servo code in `onControl()` and `onButton()` where it says **YOUR CODE HERE**.

| UI | Wire | Typical use |
|----|------|-------------|
| Left stick Y | `ly` | Throttle |
| Right stick X | `rx` | Steering |
| Right knob | `rk` | Camera pan |
| Lights | `sw` bit0 | Lights |
| BTN 2 | horn | Short buzzer pulse |
| BTN 4 | center pan | Pan to center |

## Telemetry

Every **500 ms** the board sends vehicle HUD data:

```
RC:DATA,left,<|ly|*25/10>,right,0,lo,1,ro,0,lg,1,rg,0,analog,90,batt,76,led,<sw>
```

- `left` = speed km/h × 10 (from throttle `|ly|`)
- `batt` / `analog` = demo values (76% / temp 90) until you wire sensors
- Optional `ENABLE_PLOT 1` → `RC:PLOT` ~50 ms (`v0`=temp, `v1`=`|ly|`, `v2`=batt map, `v3`=`|rx|`)

Serial inject (after handshake, Newline): paste one `RC:DATA,...` or `RC:PLOT,...` line.

## After Connect works (optional)

To hide Serial prints, set **`ENABLE_DEBUG 0`**, **`ENABLE_SERIAL_INJECT 0`**, and **`ENABLE_PLOT 0`** (if unused), then flash the sketch to the ESP32 again (Arduino IDE → Upload). You may leave them at **`1`** if you still want logs.

## Steer center trim (Android)

1. Connect with the mode in this README (Classic / BLE / SoftAP x Simple or Binary).
2. On **RC Vehicle Pro**, tap the Tune icon on the steering control.
3. Keep the stick at visual zero; use **left/right tickers** until the wheels look straight.
4. Tap **Check** — phone sends `RC:SET,steer_center,1,rx,<n>` and/or `BB 66` id `0x10`.
5. Firmware stores the current steering PWM / mix bias in **NVS** on the vehicle.
6. After reboot, stick `rx = 0` (and E-stop sticks 0) hold that mechanical straight.

Module: `SteerCenter.h` / `SteerCenter.cpp` in this sketch folder.

## Fail-safe

On Bluetooth disconnect, sticks reset to 0; with `ENABLE_DEBUG 1`, Serial prints `[IO] fail-safe`.

## Advanced

For binary protocol + fuller vehicle IO architecture, see **`../../Classic_Binary/`**.
