# RC Vehicle Pro — CAM Wi‑Fi Simple (Normal)

Oversimplified SoftAP **camera + text RC** starter for ESP32-CAM (AI-Thinker).

**RC Vehicle Pro only** (Default, one CAM). Control Panel has no Role B.

Files: `.ino` (TCP control) + `CameraStream` (`/stream` + `/capture` + `/camconfig`).

## Flash

1. Open `TeleCon_CAM_WiFi_Simple.ino` in Arduino IDE.
2. Board: **AI Thinker ESP32-CAM**.
3. Upload, open Serial Monitor at **115200**.

## SoftAP

| | |
|--|--|
| SSID | `TeleCon-RC-CAM-Starter` |
| Password | `telecon1234` |
| TCP | `192.168.4.1:3333` |
| Video | `http://192.168.4.1/stream` (MJPEG) |
| Still | `http://192.168.4.1/capture` |
| Config | `http://192.168.4.1/camconfig` (Smooth / Balanced / HQ) |

Boot camera: QQVGA, quality 28, fps 8 (Android Smooth). `ampdu_rx=0|1` re-inits SoftAP (Xiaomi workaround; invalid value → 400).

## Android

1. Phone Wi‑Fi → join the SoftAP above.
2. **RC Vehicle Pro** → Board **CAM** → **Normal** → **Wi‑Fi CAM Starter**.
3. Connect — handshake `RC:CONNECT,proto,simple` → `RC:ACK,app,RC`  
   (also accepts `proto,wifi` for SoftAP compatibility).

## What to edit

Put motor / servo code in `onControl()` and `onButton()` where it says **YOUR CODE HERE**.  
Keep camera GPIOs free; use off-board drivers on free pins if needed.

| UI | Wire | Typical use |
|----|------|-------------|
| Left stick Y | `ly` | Throttle |
| Right stick X | `rx` | Steering |
| Right knob | `rk` | Camera pan |
| Lights | `sw` bit0 | Lights |
| BTN 2 | horn | Short buzzer pulse |
| BTN 4 | center pan | Pan to center |

## Telemetry

Every **500 ms**:

```
RC:DATA,left,<|ly|*25/10>,right,0,lo,1,ro,0,lg,1,rg,0,analog,90,batt,76,led,<sw>
```

- `left` = speed km/h × 10 (from throttle `|ly|`)
- `batt` / `analog` = demo values (76% / temp 90) until you wire sensors

## After Connect works (optional)

To hide Serial prints, set **`ENABLE_DEBUG 0`** in **`DebugConfig.h`**, then flash the sketch to the ESP32 again (Arduino IDE → Upload). You may leave it at **`1`** if you still want logs.

## Steer center trim (Android)

1. Connect with the mode in this README (Classic / BLE / SoftAP x Simple or Binary).
2. On **RC Vehicle Pro**, tap the Tune icon on the steering control.
3. Keep the stick at visual zero; use **left/right tickers** until the wheels look straight.
4. Tap **Check** — phone sends `RC:SET,steer_center,1,rx,<n>` and/or `BB 66` id `0x10`.
5. Firmware stores the current steering PWM / mix bias in **NVS** on the vehicle.
6. After reboot, stick `rx = 0` (and E-stop sticks 0) hold that mechanical straight.

Module: `SteerCenter.h` / `SteerCenter.cpp` in this sketch folder.

## Fail-safe

TCP disconnect **or** ~**750 ms** without `RC:CTRL` → sticks reset; with
`ENABLE_DEBUG 1`, Serial prints `[IO] fail-safe` once on enter.

While `/stream` is busy, an idle hook keeps TCP control polling.

Stream quality on the phone (RC Vehicle Pro live screen) requires this firmware’s
`GET /camconfig` (2026-08+). Old builds without the handler: Android HUD limits only.

Arduino Serial Monitor 115200 + `ENABLE_DEBUG 1`: `[CAM] GET /camconfig …` when the
phone changes Stream quality (Smooth/Balanced/High). `ENABLE_DEBUG 0` silences it.

## Advanced

For binary protocol + modular architecture, see sibling  
**`../TeleCon_CAM_WiFi_Binary/`**.
