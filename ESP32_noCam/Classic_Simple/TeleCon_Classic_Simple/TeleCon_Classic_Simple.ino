/*
 * TeleCon RC Vehicle Pro — Classic Simple (ESP32 Dev Module, no camera)
 *
 * Android (TeleCon4ESP32):
 *   1. Pair Bluetooth "ESP32-TC-RC-BT-Simple" (PIN 1234 if asked)
 *   2. Control Panel or RC Vehicle Pro → Board DevKit → Classic + Simple
 *   3. Connect — phone sends RC:CONNECT,proto,simple
 *
 * Single-file starter for Normal users. Edit onControl() / onButton().
 * Advanced Classic Binary: ../../Classic_Binary/
 */

#include "BluetoothSerial.h"
#include "SteerCenter.h"

// === CONFIG ===
#define BT_NAME             "ESP32-TC-RC-BT-Simple"
#define BT_PIN              "1234"
#define APP_PREFIX          "RC"
#define PROTO_WIRE          "simple"
#define SERIAL_BAUD         115200

// 1 = Serial banners + stick/HS/IO logs. Set 0 when Connect works.
#define ENABLE_DEBUG        1

#if ENABLE_DEBUG
  #define DBG_PRINT(x)      Serial.print(x)
  #define DBG_PRINTLN(x)    Serial.println(x)
  #define DBG_PRINTF(...)   Serial.printf(__VA_ARGS__)
#else
  #define DBG_PRINT(x)      ((void)0)
  #define DBG_PRINTLN(x)    ((void)0)
  #define DBG_PRINTF(...)   ((void)0)
#endif

#define TELEMETRY_DATA_MS   500
#define ENABLE_PLOT         1
#define PLOT_MS             50

#define ENABLE_SERIAL_INJECT 1
#define SERIAL_MAX_LINE     192

// Demo HUD values (replace with ADC reads when you wire sensors)
#define DEMO_MOTOR_TEMP     90
#define DEMO_BATT_PCT       76

// === BLUETOOTH ===
BluetoothSerial SerialBT;

bool handshakeOk = false;
bool btWasConnected = false;

int g_lx = 0, g_ly = 0, g_rx = 0, g_ry = 0;
int g_lk = 512, g_rk = 512;
uint8_t g_sw = 0;

unsigned long lastDataMs = 0;
unsigned long lastPlotMs = 0;

#if ENABLE_SERIAL_INJECT
static String serialLineBuf;
#endif

static uint16_t lastPrintLX = 0xFFFF;
static uint16_t lastPrintLY = 0xFFFF;
static uint16_t lastPrintRX = 0xFFFF;
static uint16_t lastPrintRY = 0xFFFF;
static uint16_t lastPrintKnobL = 0xFFFF;
static uint16_t lastPrintKnobR = 0xFFFF;
static uint8_t lastPrintSw = 0xFF;

String getValue(const String& line, const char* key);
void handleLine(const String& line);
void handleConnect(const String& app, const String& line);
void sendLine(const String& line);
void resetHandshake();
void applyFailSafe();
void debugPrintControl(int lx, int ly, int rx, int ry, int lk, int rk, uint8_t sw);
void debugPrintButton(int id);
uint16_t stickTo12(int s);
#if ENABLE_SERIAL_INJECT
void serialInjectPoll();
void handleSerialInjectLine(const String& line);
#endif


// === STEER CENTER (NVS) ===
// DevKit: set ENABLE_STEER_SERVO 1 and wire a servo on PIN_STEER_SERVO.
// After Android Tune → ◀/▶ → Check, rx=0 holds that mechanical straight (NVS).
#ifndef ENABLE_STEER_SERVO
#define ENABLE_STEER_SERVO 1
#endif
#ifndef PIN_STEER_SERVO
#define PIN_STEER_SERVO 13
#endif
#ifndef CH_STEER_SERVO
#define CH_STEER_SERVO 2
#endif

#if ENABLE_STEER_SERVO
void steerServoBegin() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(PIN_STEER_SERVO, 50, 12);
  ledcWrite(PIN_STEER_SERVO, steerCenterDuty12FromUs(steerCenterGetUs()));
#else
  ledcSetup(CH_STEER_SERVO, 50, 12);
  ledcAttachPin(PIN_STEER_SERVO, CH_STEER_SERVO);
  ledcWrite(CH_STEER_SERVO, steerCenterDuty12FromUs(steerCenterGetUs()));
#endif
}

void steerServoWriteRx(int rx) {
  uint32_t duty = steerCenterDuty12FromRx(rx);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(PIN_STEER_SERVO, duty);
#else
  ledcWrite(CH_STEER_SERVO, duty);
#endif
}

void steerServoHoldCenter() {
  uint32_t duty = steerCenterDuty12FromUs(steerCenterGetUs());
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(PIN_STEER_SERVO, duty);
#else
  ledcWrite(CH_STEER_SERVO, duty);
#endif
}
#else
void steerServoBegin() {}
void steerServoWriteRx(int rx) { steerCenterNoteApplied(rx); }
void steerServoHoldCenter() {}
#endif

void onControl(int lx, int ly, int rx, int ry, int lk, int rk, uint8_t sw) {
  debugPrintControl(lx, ly, rx, ry, lk, rk, sw);

  // Steering map uses NVS center — trim ticks move the servo in tiny steps.
  steerServoWriteRx(rx);
  // Optional differential mix: int bias = steerCenterMapBias(rx);

  // YOUR CODE HERE — drive motors / other actuators from sticks & knobs
  //   ly = throttle (-100..100), rx = steering, rk = pan, sw bit0 = lights
  (void)lx; (void)ly; (void)ry; (void)lk; (void)rk; (void)sw;
}

void onButton(int id) {
  debugPrintButton(id);

  // YOUR CODE HERE — button actions
  if (id == 2) {
    // Horn note — pulse buzzer briefly
  } else if (id == 4) {
    // Center pan note — move camera pan servo to center
  } else if (id == STEER_CENTER_BTN_ID) {
    steerCenterSaveFromCurrentRx(g_rx);
    steerServoHoldCenter();
    sendLine("RC:ACK,steer_center,1");
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(200);

  steerCenterBegin();
  steerServoBegin();

  // Legacy PIN pairing only (Android often fails if SSP + setPin are mixed).
  SerialBT.disableSSP();
  SerialBT.setPin(BT_PIN, 4);

  // isMaster=false, disableBLE=true → Classic SPP only
  if (!SerialBT.begin(BT_NAME, false, true)) {
    Serial.println("Bluetooth init failed!");
    while (true) { delay(1000); }
  }

  DBG_PRINTLN();
  DBG_PRINTLN("=== TeleCon RC Vehicle Pro — Classic Simple ===");
  DBG_PRINT("BT name: ");
  DBG_PRINTLN(BT_NAME);
  DBG_PRINTLN("Pair with PIN 1234 if the phone asks.");
  DBG_PRINTLN("Android: RC Vehicle Pro → Board DevKit → Normal → Classic + Simple");
  DBG_PRINTLN("Handshake: RC:CONNECT,proto,simple  →  RC:ACK,app,RC");
  DBG_PRINTLN("Examples:");
  DBG_PRINTLN("  RC:CONNECT,proto,simple");
  DBG_PRINTLN("  RC:CTRL,lx,0,ly,0,rx,0,ry,0,lk,512,rk,512,sw,00");
  DBG_PRINTLN("  RC:BTN,id,1");
  DBG_PRINTLN("  RC:SET,steer_center,1,rx,0");
  DBG_PRINTLN("Steer trim: Android Tune → ◀/▶ → Check (center in NVS).");
  DBG_PRINTLN("Edit onControl() / onButton() at YOUR CODE HERE.");
  DBG_PRINTLN("Telemetry: RC:DATA every 500 ms (speed×10 from |ly|, demo batt/temp).");
#if ENABLE_PLOT
  DBG_PRINTLN("Plot ON — RC:PLOT ~50 ms (v0=temp, v1=|ly|, v2=batt, v3=|rx|).");
#else
  DBG_PRINTLN("Plot OFF — set ENABLE_PLOT 1 to send RC:PLOT.");
#endif
#if ENABLE_SERIAL_INJECT
  DBG_PRINTLN("Serial inject @ 115200 (after handshake, line ending = Newline):");
  DBG_PRINTLN("  RC:DATA,left,186,right,0,lo,1,ro,0,lg,1,rg,0,analog,90,batt,76,led,01");
  DBG_PRINTLN("  RC:PLOT,v0,128,v1,200,v2,64,v3,180");
  DBG_PRINTLN("  Inject is one-shot (pulse); auto telemetry resumes after.");
#endif
  DBG_PRINTLN();
}

void loop() {
  steerCenterLoop();
  bool connected = SerialBT.hasClient();
  if (connected != btWasConnected) {
    btWasConnected = connected;
    if (connected) {
      resetHandshake();
      DBG_PRINTLN("[BT] connected — waiting for RC:CONNECT,proto,simple");
    } else {
      applyFailSafe();
      resetHandshake();
      DBG_PRINTLN("[BT] disconnected");
    }
  }

  while (SerialBT.available()) {
    String line = SerialBT.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      handleLine(line);
    }
  }

#if ENABLE_SERIAL_INJECT
  serialInjectPoll();
#endif

  if (!handshakeOk) return;

  unsigned long now = millis();

  if (now - lastDataMs >= TELEMETRY_DATA_MS) {
    lastDataMs = now;

    int speedX10 = abs(g_ly) * 25 / 10;
    int analog = DEMO_MOTOR_TEMP;
    int batt = DEMO_BATT_PCT;

    char data[128];
    snprintf(data, sizeof(data),
      "RC:DATA,left,%d,right,0,lo,1,ro,0,lg,1,rg,0,analog,%d,batt,%d,led,%02X",
      speedX10, analog, batt, g_sw);
    sendLine(data);
  }

#if ENABLE_PLOT
  if (now - lastPlotMs >= PLOT_MS) {
    lastPlotMs = now;

    int v0 = DEMO_MOTOR_TEMP;
    int v1 = constrain(map(abs(g_ly), 0, 100, 0, 255), 0, 255);
    int v2 = constrain(map(DEMO_BATT_PCT, 0, 100, 0, 255), 0, 255);
    int v3 = constrain(map(abs(g_rx), 0, 100, 0, 255), 0, 255);
    char plot[64];
    snprintf(plot, sizeof(plot), "RC:PLOT,v0,%d,v1,%d,v2,%d,v3,%d", v0, v1, v2, v3);
    sendLine(plot);
  }
#endif
}

#if ENABLE_SERIAL_INJECT
void handleSerialInjectLine(const String& line) {
  if (!SerialBT.hasClient()) {
    DBG_PRINTLN("[SERIAL] no BT client — line not sent");
    return;
  }
  if (!handshakeOk) {
    DBG_PRINTLN("[SERIAL] waiting for RC:CONNECT handshake before inject");
    return;
  }

  if (line.startsWith("RC:DATA") || line.startsWith("RC:PLOT")) {
    sendLine(line);
    DBG_PRINT("[SERIAL] inject one-shot → phone: ");
    DBG_PRINTLN(line.startsWith("RC:DATA") ? "RC:DATA" : "RC:PLOT");
    return;
  }

  DBG_PRINT("[SERIAL] unhandled inject (use RC:DATA / RC:PLOT): ");
  DBG_PRINTLN(line);
}

void serialInjectPoll() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n') {
      if (serialLineBuf.length() > 0) {
        serialLineBuf.trim();
        if (serialLineBuf.length() > 0) {
          handleSerialInjectLine(serialLineBuf);
        }
        serialLineBuf = "";
      }
    } else if (c != '\r') {
      if ((int)serialLineBuf.length() < SERIAL_MAX_LINE) {
        serialLineBuf += c;
      } else {
        serialLineBuf = "";
        DBG_PRINTLN("[SERIAL] line too long, discarded");
      }
    }
  }
}
#endif

uint16_t stickTo12(int s) {
  return (uint16_t)(((constrain(s, -100, 100) + 100) * 4095) / 200);
}

void debugPrintControl(int lx, int ly, int rx, int ry, int lk, int rk, uint8_t sw) {
  uint16_t lx12 = stickTo12(lx);
  uint16_t ly12 = stickTo12(ly);
  uint16_t rx12 = stickTo12(rx);
  uint16_t ry12 = stickTo12(ry);
  uint16_t lkU = (uint16_t)constrain(lk, 0, 1023);
  uint16_t rkU = (uint16_t)constrain(rk, 0, 1023);

  if (lx12 != lastPrintLX) {
    DBG_PRINTF("L Stick X: %u    ", lx12);
    DBG_PRINTF("L Stick Y: %u\n", ly12);
    lastPrintLX = lx12;
  }
  if (ly12 != lastPrintLY) {
    DBG_PRINTF("L Stick X: %u    ", lx12);
    DBG_PRINTF("L Stick Y: %u\n", ly12);
    lastPrintLY = ly12;
  }
  if (rx12 != lastPrintRX) {
    DBG_PRINTF("R Stick X: %u    ", rx12);
    DBG_PRINTF("R Stick Y: %u\n", ry12);
    lastPrintRX = rx12;
  }
  if (ry12 != lastPrintRY) {
    DBG_PRINTF("R Stick X: %u    ", rx12);
    DBG_PRINTF("R Stick Y: %u\n", ry12);
    lastPrintRY = ry12;
  }
  if (lkU != lastPrintKnobL) {
    DBG_PRINTF("Left Knob: %u\n", lkU);
    lastPrintKnobL = lkU;
  }
  if (rkU != lastPrintKnobR) {
    DBG_PRINTF("Right Knob: %u\n", rkU);
    lastPrintKnobR = rkU;
  }
  if (sw != lastPrintSw) {
    for (int i = 0; i < 6; i++) {
      byte bit = (1 << i);
      if ((sw & bit) != (lastPrintSw & bit)) {
        DBG_PRINTF("SW%d = %d\n", i + 1, (sw & bit) ? 1 : 0);
      }
    }
    lastPrintSw = sw;
  }
}

void debugPrintButton(int id) {
  DBG_PRINTF("BTN%d : Pressed\n", id);
}

void sendLine(const String& line) {
  SerialBT.print(line);
  SerialBT.print('\n');
}

void applyFailSafe() {
  g_lx = g_ly = g_rx = g_ry = 0;
  g_lk = g_rk = 512;
  g_sw = 0;
  DBG_PRINTLN("[IO] fail-safe");
  // YOUR CODE HERE — stop motors on disconnect
}

void resetHandshake() {
  handshakeOk = false;
  lastPrintLX = lastPrintLY = lastPrintRX = lastPrintRY = 0xFFFF;
  lastPrintKnobL = lastPrintKnobR = 0xFFFF;
  lastPrintSw = 0xFF;
}

String getValue(const String& line, const char* key) {
  int colon = line.indexOf(':');
  if (colon < 0) return "";

  int pos = line.indexOf(',', colon);
  if (pos < 0) return "";
  pos++;

  while (pos < (int)line.length()) {
    int keyEnd = line.indexOf(',', pos);
    if (keyEnd < 0) break;

    String k = line.substring(pos, keyEnd);
    int valEnd = line.indexOf(',', keyEnd + 1);
    String v = (valEnd < 0)
      ? line.substring(keyEnd + 1)
      : line.substring(keyEnd + 1, valEnd);

    if (k == key) return v;
    pos = (valEnd < 0) ? line.length() : valEnd + 1;
  }
  return "";
}

void handleConnect(const String& app, const String& line) {
  if (!app.equalsIgnoreCase(APP_PREFIX)) {
    sendLine(String("RC:NAK,reason,app_mismatch,expected,") + APP_PREFIX +
             ",actual," + app);
    handshakeOk = false;
    return;
  }

  String proto = getValue(line, "proto");
  if (proto.length() == 0) {
    sendLine(String("RC:NAK,reason,proto_mismatch,expected,") + PROTO_WIRE +
             ",actual,unknown");
    handshakeOk = false;
    return;
  }

  if (proto.equalsIgnoreCase(PROTO_WIRE)) {
    sendLine("RC:ACK,app,RC");
    handshakeOk = true;
    DBG_PRINTLN("[HS] ACK — session ready");
    return;
  }

  sendLine(String("RC:NAK,reason,proto_mismatch,expected,") + PROTO_WIRE +
           ",actual," + proto);
  handshakeOk = false;
}

void handleLine(const String& line) {
  int colon = line.indexOf(':');
  if (colon <= 0) return;

  String app = line.substring(0, colon);
  String rest = line.substring(colon + 1);
  int comma = rest.indexOf(',');
  String type = (comma < 0) ? rest : rest.substring(0, comma);

  if (type.equalsIgnoreCase("CONNECT")) {
    handleConnect(app, line);
    return;
  }

  if (!handshakeOk) return;
  if (!app.equalsIgnoreCase("RC")) return;

  if (type.equalsIgnoreCase("CTRL")) {
    g_lx = getValue(line, "lx").toInt();
    g_ly = getValue(line, "ly").toInt();
    g_rx = getValue(line, "rx").toInt();
    g_ry = getValue(line, "ry").toInt();
    g_lk = getValue(line, "lk").toInt();
    g_rk = getValue(line, "rk").toInt();
    String swHex = getValue(line, "sw");
    g_sw = (uint8_t)strtol(swHex.c_str(), nullptr, 16);

    onControl(g_lx, g_ly, g_rx, g_ry, g_lk, g_rk, g_sw);
  } else if (type.equalsIgnoreCase("BTN")) {
    onButton(getValue(line, "id").toInt());
  } else if (type.equalsIgnoreCase("SET")) {
    if (steerCenterHandleSetLine(line, g_rx)) {
      steerServoHoldCenter();
      sendLine("RC:ACK,steer_center,1");
    }
  }
}
