# TeleCon — SoftAP video only (ESP32-CAM, Role A overlay)

Shared by **Control Panel** and **RC Vehicle Pro**.

ESP32-CAM firmware for **camera SoftAP only**.  
**No** TCP `:3333`, **no** Classic/BLE on this board.

Use with a separate DevKit Bluetooth sketch for sticks/telemetry. Works with **any** of:

| DevKit control | Sketch | Name |
|----------------|--------|------|
| Classic Simple | `ESP32_noCam/Classic_Simple/TeleCon_Classic_Simple/` | `ESP32-TC-RC-BT-Simple` |
| Classic Binary | `ESP32_noCam/Classic_Binary/TeleCon_Classic_Binary/` | `ESP32-TC-RC-BT-Binary` |
| BLE Binary | `ESP32_noCam/BLE/TeleCon_BLE_Binary/` | `ESP32-TC-RC-BLE-Binary` |

## SoftAP

| Item | Value |
|------|--------|
| SSID | `TeleCon-RC-CAM-Starter` |
| Password | `telecon1234` |
| IP | `192.168.4.1` |

## Endpoints

| Service | How |
|---------|-----|
| MJPEG (preferred) | `GET http://192.168.4.1/stream` |
| Single JPEG | `GET http://192.168.4.1/capture` |
| Stream quality | `GET http://192.168.4.1/camconfig?framesize=&quality=&fps=` |

Boot camera: VGA, quality 15, fps 12 (Android Balanced). `ampdu_rx=0|1` re-inits SoftAP (Xiaomi workaround; invalid value → 400).

## Android

1. Join SoftAP `TeleCon-RC-CAM-Starter`.
2. App: keep **Classic Simple**, **Classic Binary**, or **BLE Binary** for the DevKit (CAM does not change protocol).
3. Enable SoftAP camera overlay; Connect targets the **DevKit**, not SoftAP TCP.
4. Camera pane uses `/stream`.

Control Panel has no one-CAM Role B. RC Vehicle Pro Role B uses a different sketch (video + TCP).

## Flash

1. Board: **AI Thinker ESP32-CAM**
2. Open this folder’s `.ino`
3. Upload; Serial @ **115200**

## Need camera + control on one CAM? (RC Vehicle Pro only)

Use SoftAP video **and** SoftAP TCP control (no extra DevKit):

| Protocol | Sketch |
|----------|--------|
| Simple (Default) | `ESP32_cam/WiFi/TeleCon_CAM_WiFi_Simple/` |
| Binary (Advanced) | `ESP32_cam/WiFi/TeleCon_CAM_WiFi_Binary/` |
