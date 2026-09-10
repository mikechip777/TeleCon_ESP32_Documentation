# RC Vehicle Pro — CAM Wi‑Fi Binary (Advanced)

ESP32-CAM SoftAP **video + binary RC control** on one board (AI-Thinker).

**RC Vehicle Pro only** (Advanced, one CAM). Control Panel has no Role B.

SoftAP is started once by `CameraStream`; `TeleConWifi` attaches TCP `:3333` only.

## Flash

1. Open `TeleCon_CAM_WiFi_Binary.ino` in Arduino IDE.
2. Board: **AI Thinker ESP32-CAM** (or ESP32 Wrover Module with PSRAM).
3. Upload, open Serial Monitor at **115200**.

## SoftAP

| | |
|--|--|
| SSID | `TeleCon-RC-CAM` |
| Password | `telecon1234` |
| TCP | `192.168.4.1:3333` |
| Video | `http://192.168.4.1/stream` (MJPEG) |
| Still | `http://192.168.4.1/capture` |
| Config | `http://192.168.4.1/camconfig` |

Boot camera: QQVGA, quality 28, fps 8 (Android Smooth). `ampdu_rx=0|1` re-inits SoftAP (Xiaomi workaround; invalid value → 400).

Matches Advanced Kit A SoftAP (`TeleCon-RC-CAM`).

## Android settings

**RC Vehicle Pro** → Board **CAM** → **Advanced** → **Wi‑Fi SoftAP Binary**

> Android must use **`proto=binary`** (`AA55` / `BB66` / `CC11/CC22`). The old CAM text path
> (`RC:CONNECT,proto,wifi` + `RC:CTRL` lines) is superseded by this sketch.

Join SoftAP, then Connect.

## Protocol

- Handshake: `RC:CONNECT,proto,binary` → `RC:ACK,app,RC` / `RC:NAK,...`
- Control: `AA 55` (18 B) + `BB 66` buttons
- Telemetry: `CC 11` / `CC 22` (~2 Hz) + optional `CC 33` debug plot (~20 Hz)
- HTTP: `/stream`, `/capture`, `/camconfig` (Smooth / Balanced / HQ), `/status`

Stream quality on the phone requires firmware with `/camconfig` (2026-08+).

Arduino Serial Monitor 115200 + `TELECON_DEBUG 1`: `[CAM] GET /camconfig …` when the
phone changes Stream quality (Smooth/Balanced/High). `TELECON_DEBUG 0` silences it.

While `/stream` holds the WebServer client, an idle hook keeps TCP RC polling.

## Config highlights

| Macro | Value |
|-------|--------|
| `TELECON_PROTO_WIRE` | `"binary"` |
| `TELECON_VEHICLE_IO` | `0` (camera pins free) |
| `TELECON_HAS_CAMERA` | `1` |
| `TELECON_AP_SSID` / `TELECON_CAM_AP_SSID` | `TeleCon-RC-CAM` |
| `TELECON_WIFI_CTRL_PORT` | `3333` |
| `TELECON_WIFI_OWN_SOFTAP` | `0` (CameraStream owns SoftAP) |

## Modules

`TeleConConfig` · `TeleConWifi` · `TeleConBinaryRx` · `RcVehicleHandlers` · `RcVehicleControl` · `CameraStream` · `CameraStreamCamConfig` · `pins` · `.ino`

## Steer center trim (Android)

1. Connect with the mode in this README (Classic / BLE / SoftAP x Simple or Binary).
2. On **RC Vehicle Pro**, tap the Tune icon on the steering control.
3. Keep the stick at visual zero; use **left/right tickers** until the wheels look straight.
4. Tap **Check** — phone sends `RC:SET,steer_center,1,rx,<n>` and/or `BB 66` id `0x10`.
5. Firmware stores the current steering PWM / mix bias in **NVS** on the vehicle.
6. After reboot, stick `rx = 0` (and E-stop sticks 0) hold that mechanical straight.

Module: `SteerCenter.h` / `SteerCenter.cpp` in this sketch folder.

## Fail-safe

TCP disconnect → `rcVehicleFailSafe()` (prints once on enter). Motors are off while `TELECON_VEHICLE_IO=0`.

## Normal starter

For an oversimplified text-control CAM sketch, see sibling **`../TeleCon_CAM_WiFi_Simple/`**.
