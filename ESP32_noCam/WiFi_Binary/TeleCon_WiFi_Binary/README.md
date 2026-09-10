# TeleCon — Wi‑Fi Binary (Advanced)

Complete architecture for **Advanced** DevKit users.  
DevKit SoftAP Simple is withdrawn. Default (no camera) is Classic Simple.

## Board

ESP32 DevKit — SoftAP TCP control (no camera HTTP).

## SoftAP

| | |
|--|--|
| SSID | `ESP32-TC-RC-WiFi-Binary` |
| Password | `telecon1234` |
| TCP | `192.168.4.1:3333` |

Matches Android `Esp32DevKitSoftApDefaults` for app prefix `RC` + **Wi‑Fi SoftAP + Binary**.

## Android settings

**RC Vehicle Pro** → Board **DevKit** → Connection type **Wi‑Fi SoftAP + Binary**  
Join the SoftAP above, then Connect.

## Protocol

- Handshake: `RC:CONNECT,proto,binary` → `RC:ACK,app,RC` / `RC:NAK,...`
- Control: `AA 55` (18 B) + `BB 66` buttons
- Telemetry: `CC 11` / `CC 22` (~2 Hz) + optional `CC 33` debug plot (~20 Hz)
- No `CC 44` / `RC:PLOTCFG` (labels stay in Android)

## HUD mapping

| HUD | Packet | Convention |
|-----|--------|------------|
| Speed km/h | `CC 11` left | speed×10 |
| Battery % | `CC 22` batt | 0–100 |
| Motor temp | `CC 22` analog | 0–255 |

## Control map

| UI | Wire | Action |
|----|------|--------|
| Left stick Y | `ly` | Throttle |
| Right stick X | `rx` | Steering |
| Right knob | `rk` | Pan servo |
| Lights | `sw` bit0 | Lights |
| Buzzer | BTN 2 | Horn |
| Camera front | BTN 4 | Center pan |

## Serial inject (after handshake)

```
RC:DATA,left,186,right,0,lo,1,ro,0,lg,1,rg,0,analog,90,batt,76,led,01
RC:PLOT,v0,128,v1,200,v2,64,v3,180
```

Encoded to one-shot `CC 11`/`CC 22`/`CC 33` (Control Panel style).

## Steer center trim (Android)

1. Connect with the mode in this README (Wi‑Fi SoftAP + Binary).
2. On **RC Vehicle Pro**, tap the Tune icon on the steering control.
3. Keep the stick at visual zero; use **left/right tickers** until the wheels look straight.
4. Tap **Check** — phone sends `RC:SET,steer_center,1,rx,<n>` and/or `BB 66` id `0x10`.
5. Firmware stores the current steering PWM / mix bias in **NVS** on the vehicle.
6. After reboot, stick `rx = 0` (and E-stop sticks 0) hold that mechanical straight.

Module: `SteerCenter.h` / `SteerCenter.cpp` in this sketch folder.

## Fail-safe

TCP disconnect → `rcVehicleFailSafe()` zeros motors.
