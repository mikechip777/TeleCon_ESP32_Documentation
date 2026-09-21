# TeleCon ESP32 firmware

Shared `RC:` sketches for Android **Control Panel** and **RC Vehicle Pro**. Role B (one CAM video + TCP) is RC Vehicle only.

Programmers writing firmware for **any** Bluetooth or TCP device (not only these sketches): see [Protocol_Documentation](Protocol_Documentation/README.md).

Android app users (Home, first connection, Control panel, RC Vehicle): see [General_Documentation](General_Documentation/README.md) (full user guide + fast guide).

Open a sketch folder in Arduino IDE (the folder that contains the `.ino`).
Pin maps, BT/SSID identity, and `YOUR CODE HERE` stay in that sketch's `README.md`.

## Product matrix

| Sketch | Control Panel | RC Vehicle Pro | Path |
|--------|---------------|----------------|------|
| DevKit Classic Simple | Yes | Yes | `ESP32_noCam/Classic_Simple/TeleCon_Classic_Simple/` |
| DevKit Classic Binary | Yes | Yes | `ESP32_noCam/Classic_Binary/TeleCon_Classic_Binary/` |
| DevKit BLE Binary | Yes | Yes | `ESP32_noCam/BLE/TeleCon_BLE_Binary/` |
| DevKit Wi‑Fi Binary | Yes | Yes | `ESP32_noCam/WiFi_Binary/TeleCon_WiFi_Binary/` |
| CAM video-only (Role A overlay) | Yes, with DevKit BT | Yes, with DevKit BT | `ESP32_cam/WiFi/TeleCon_CAM_SoftAP_Video/` |
| CAM video+control Simple | **No** | Yes (Default, one CAM) | `ESP32_cam/WiFi/TeleCon_CAM_WiFi_Simple/` |
| CAM video+control Binary | **No** | Yes (Advanced, one CAM) | `ESP32_cam/WiFi/TeleCon_CAM_WiFi_Binary/` |

## Identities

| Build | Name / SSID | Handshake |
|-------|-------------|-----------|
| Classic Simple | BT `ESP32-TC-RC-BT-Simple` | `proto,simple` |
| Classic Binary | BT `ESP32-TC-RC-BT-Binary` | `proto,binary` |
| BLE Binary | `ESP32-TC-RC-BLE-Binary` | `proto,binary` |
| DevKit Wi‑Fi Binary | SoftAP `ESP32-TC-RC-WiFi-Binary` | `proto,binary` |
| CAM video-only (Role A) | SoftAP `TeleCon-RC-CAM-Starter` | HTTP `/stream` only |
| CAM Simple (Role B, Normal) | SoftAP `TeleCon-RC-CAM-Starter` | `proto,simple` |
| CAM Binary (Role B, Advanced) | SoftAP `TeleCon-RC-CAM` | `proto,binary` |

Password for SoftAP builds: `telecon1234`. Control TCP (Role B and DevKit Wi‑Fi Binary): `192.168.4.1:3333`.

## Steer center trim

On **RC Vehicle Pro**, Tune → tickers → Check. Firmware in each steering sketch stores the bias in NVS (`SteerCenter` in that sketch folder).
