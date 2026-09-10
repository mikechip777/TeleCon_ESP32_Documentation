#include "TeleConBluetooth.h"
#include "TeleConConfig.h"
#include "TeleConDebug.h"
#include "TeleConProtocol.h"
#include "RcVehicleHandlers.h"
#include "TeleConBinaryRx.h"
#include "RcVehicleControl.h"

BluetoothSerial SerialBT;
#if TELECON_SERIAL_INJECT
static String serialLineBuffer;
#endif

static String lastRxLogLine;
static bool btClientConnected = false;

static void writeLineToBluetooth(const char* line) {
  SerialBT.print(line);
  SerialBT.print('\n');
}

static void logAndSendLine(const char* line) {
#if TELECON_DEBUG
  DBG_PRINT("[TX] ");
  DBG_PRINTLN(line);
#endif
  writeLineToBluetooth(line);
}

void teleconSendLineResetCache() {
  lastRxLogLine = "";
}

bool teleconSendLine(const char* line) {
  logAndSendLine(line);
  return true;
}

bool teleconSendLine(const String& line) {
  return teleconSendLine(line.c_str());
}

void teleconSendLineForce(const char* line) {
  logAndSendLine(line);
}

void teleconSendLineForce(const String& line) {
  teleconSendLineForce(line.c_str());
}

bool teleconSendBinary(const uint8_t* data, size_t len) {
  if (!SerialBT.hasClient() || data == nullptr || len == 0) return false;
  size_t n = SerialBT.write(data, len);
  return n == len;
}

void onLineReceived(const String& line) {
  String app, type;
  if (!parseLine(line, app, type)) {
#if TELECON_DEBUG
    if (line != lastRxLogLine) {
      lastRxLogLine = line;
      DBG_PRINTLN("[RX] malformed line");
      DBG_PRINTLN(line);
    }
#endif
    return;
  }

#if TELECON_DEBUG
  if (line != lastRxLogLine || type == "BTN") {
    lastRxLogLine = line;
    DBG_PRINT("[RX] ");
    DBG_PRINTLN(line);
  }
#endif

  if (type == "CONNECT") {
    handleRcConnect(app, line);
    return;
  }

  if (app == "RC" && type == "CTRL") {
    handleRcCtrlText(line);
  } else if (app == "RC" && type == "BTN") {
    handleRcBtnText(line);
  } else if (app == "RC" && type == "SET") {
    handleRcSetText(line);
  }
#if TELECON_DEBUG
  else {
    DBG_PRINT("[RX] unhandled: ");
    DBG_PRINTLN(line);
  }
#endif
}

static void onBluetoothClientChanged(bool connected) {
  btClientConnected = connected;
  rcVehicleHandshakeReset();
  teleconBinaryRxReset();
  if (connected) {
    teleconSendLineResetCache();
    rcVehicleHandlersReset();
#if TELECON_DEBUG
    DBG_PRINTLN("[BT] client connected — waiting for RC:CONNECT,proto,binary");
#endif
  } else {
#if TELECON_DEBUG
    DBG_PRINTLN("[BT] client disconnected");
#endif
  }
}

bool teleconBluetoothBegin() {
#if TELECON_SERIAL_INJECT
  serialLineBuffer.reserve(TELECON_LINE_RESERVE);
#endif
  lastRxLogLine = "";
  btClientConnected = false;
  rcVehicleHandshakeReset();
  teleconSendLineResetCache();
  teleconBinaryRxReset();

  // Android (e.g. POCO) often fails when SSP + setPin are mixed ("PIN incorrect").
  // Use legacy PIN pairing only; enter 1234 if the phone asks.
  SerialBT.disableSSP();
  SerialBT.setPin("1234", 4);

  // isMaster=false, disableBLE=true → Classic SPP only (frees radio / clearer SDP).
  bool ok = SerialBT.begin(TELECON_BT_NAME, false, true);
#if TELECON_DEBUG
  if (ok) {
    DBG_PRINTF("[BT] Classic SPP ready as %s (pair with PIN 1234)\n", TELECON_BT_NAME);
  } else {
    DBG_PRINTLN("[BT] SerialBT.begin failed");
  }
#endif
  return ok;
}

void teleconSerialPoll() {
#if TELECON_SERIAL_INJECT
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      if (serialLineBuffer.length() > 0) {
        serialLineBuffer.trim();
        if (serialLineBuffer.length() > 0) {
          // Encode RC:DATA / RC:PLOT to CC frames (Control Panel inject style).
          rcVehicleHandleSerialInjectLine(serialLineBuffer);
        }
        serialLineBuffer = "";
      }
    } else if (c != '\r') {
      if (serialLineBuffer.length() < TELECON_MAX_LINE) {
        serialLineBuffer += c;
      } else {
        serialLineBuffer = "";
      }
    }
  }
#endif
}

void teleconBluetoothPoll() {
  bool hasClient = SerialBT.hasClient();
  if (hasClient != btClientConnected) {
    onBluetoothClientChanged(hasClient);
  }
  while (SerialBT.available()) {
    teleconBinaryRxFeed((uint8_t)SerialBT.read());
  }
}
