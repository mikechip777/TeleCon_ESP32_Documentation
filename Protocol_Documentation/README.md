# TeleCon control protocol (any MCU)

Technical firmware reference (v1.2): transport, handshake, Simple/Binary
payloads, plus Classic Simple implementations (ESP32 `BluetoothSerial`
and HC-05 UART).

Official Arduino sketches stay under `ESP32_noCam/` and `ESP32_cam/`.

## Folder layout

Each language is its own container (same `.tex` file name):

| Language | Source | PDF |
|----------|--------|-----|
| English | `en/src/telecon_control_protocol.tex` | `en/build/telecon_control_protocol.pdf` |
| Spanish | `es/src/telecon_control_protocol.tex` | `es/build/telecon_control_protocol.pdf` |

## Build PDF

```bash
cd en/src
pdflatex -output-directory=../build telecon_control_protocol.tex
pdflatex -output-directory=../build telecon_control_protocol.tex

cd ../../es/src
pdflatex -output-directory=../build telecon_control_protocol.tex
pdflatex -output-directory=../build telecon_control_protocol.tex
```
