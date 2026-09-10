/*
 * TeleConBle.cpp — NUS GATT server: advertising, RX → BinaryRx, chunked TX.
 *
 * NUS GATT server for RC Control Panel / RC Vehicle binary firmware.
 */

#include "TeleConBle.h"
#include "TeleConBinaryRx.h"
#include "TeleConConfig.h"
#include "TeleConProtocol.h"
#include "RcVehicleHandlers.h"
#include "pins.h"

#include <esp_bt.h>
#include <string.h>
#include <string>

#if TELECON_USE_NIMBLE
#include <NimBLEDevice.h>
using BleServer = NimBLEServer;
using BleCharacteristic = NimBLECharacteristic;
using BleAdvertising = NimBLEAdvertising;
#else
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
using BleServer = BLEServer;
using BleCharacteristic = BLECharacteristic;
using BleAdvertising = BLEAdvertising;
#endif

namespace {

BleServer* bleServer = nullptr;
BleCharacteristic* txCharacteristic = nullptr;
volatile bool deviceConnected = false;
volatile bool prevDeviceConnected = false;
volatile bool notificationsEnabled = false;
bool advertising = false;
String lastRxLogLine;

#if TELECON_SERIAL_INJECT
String serialLineBuffer;
#endif

size_t notifyChunkSize() {
  size_t chunk = (size_t)TELECON_NOTIFY_CHUNK;
  if (chunk < 1) chunk = BLE_DEFAULT_CHUNK;
  if (chunk > 244) chunk = 244;
  return chunk;
}

bool bleNotifyBytes(const uint8_t* data, size_t len) {
  if (!deviceConnected || !notificationsEnabled || txCharacteristic == nullptr ||
      data == nullptr || len == 0) {
    return false;
  }
  const size_t chunk = notifyChunkSize();
  size_t offset = 0;
  while (offset < len) {
    size_t n = len - offset;
    if (n > chunk) n = chunk;
    txCharacteristic->setValue(const_cast<uint8_t*>(data + offset), n);
    txCharacteristic->notify();
    offset += n;
    if (offset < len) {
      delay(4);
    }
  }
  return true;
}

void onBleClientChanged(bool connected) {
  rcVehicleHandshakeReset();
  teleconBinaryRxReset();
  if (connected) {
    rcVehicleHandlersReset();
    DBG_PRINTLN("[BLE] client connected — waiting for RC:CONNECT,proto,binary");
  } else {
    DBG_PRINTLN("[BLE] client disconnected");
  }
}

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BleServer* server) override {
    deviceConnected = true;
    notificationsEnabled = false;
    teleconBleStopAdvertising();
    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, HIGH);
    onBleClientChanged(true);
    (void)server;
  }

  void onDisconnect(BleServer* server) override {
    deviceConnected = false;
    notificationsEnabled = false;
    digitalWrite(PIN_STATUS_LED, LOW);
    onBleClientChanged(false);
    (void)server;
  }
};

class RxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BleCharacteristic* characteristic) override {
#if TELECON_USE_NIMBLE
    std::string value = characteristic->getValue();
    const uint8_t* data = reinterpret_cast<const uint8_t*>(value.data());
    size_t len = value.length();
#elif defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    String value = characteristic->getValue();
    const uint8_t* data = reinterpret_cast<const uint8_t*>(value.c_str());
    size_t len = value.length();
#else
    std::string value = characteristic->getValue();
    const uint8_t* data = reinterpret_cast<const uint8_t*>(value.data());
    size_t len = value.length();
#endif
    if (data == nullptr || len == 0) return;
    for (size_t i = 0; i < len; i++) {
      teleconBinaryRxFeed(data[i]);
    }
  }
};

#if TELECON_USE_NIMBLE
class TxCallbacks : public NimBLECharacteristicCallbacks {
  void onSubscribe(NimBLECharacteristic* characteristic, ble_gap_conn_desc* desc,
                   uint16_t subValue) override {
    notificationsEnabled = (subValue & 0x0001) != 0;
    DBG_PRINTF("[BLE] CCCD notify=%d\n", notificationsEnabled ? 1 : 0);
    (void)characteristic;
    (void)desc;
  }
};
#endif

}  // namespace

void onLineReceived(const String& line) {
  String app, type;
  if (!parseLine(line, app, type)) {
    if (line != lastRxLogLine) {
      lastRxLogLine = line;
      DBG_PRINTLN("[RX] malformed line");
      DBG_PRINTLN(line);
    }
    return;
  }

  if (line != lastRxLogLine) {
    lastRxLogLine = line;
    DBG_PRINT("[RX] ");
    DBG_PRINTLN(line);
  }

  if (type == "CONNECT") {
    handleRcConnect(app, line);
    return;
  }

  // BLE prefers binary; accept text CTRL/BTN for Serial inject / benches
  if (app == TELECON_APP_PREFIX && type == "CTRL") {
    handleRcCtrlText(line);
    return;
  }
  if (app == TELECON_APP_PREFIX && type == "BTN") {
    handleRcBtnText(line);
    return;
  }
  if (app == TELECON_APP_PREFIX && type == "SET") {
    handleRcSetText(line);
    return;
  }

  DBG_PRINT("[RX] unhandled: ");
  DBG_PRINTLN(line);
}

void teleconBleStartAdvertising() {
  BleAdvertising* adv =
#if TELECON_USE_NIMBLE
      NimBLEDevice::getAdvertising();
#else
      BLEDevice::getAdvertising();
#endif
  if (adv == nullptr) return;

#if TELECON_USE_NIMBLE
  adv->reset();
  adv->setName(TELECON_BLE_NAME);
  adv->addServiceUUID(NUS_SERVICE_UUID);
  adv->enableScanResponse(true);
  adv->setMinPreferred(0x06);
  adv->setMaxPreferred(0x12);
  adv->start();
#else
  adv->addServiceUUID(NUS_SERVICE_UUID);
  adv->setScanResponse(true);
  adv->setMinPreferred(0x06);
  adv->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();
#endif
  advertising = true;
  DBG_PRINTF("[BLE] advertising as %s\n", TELECON_BLE_NAME);
}

void teleconBleStopAdvertising() {
  if (!advertising) return;
#if TELECON_USE_NIMBLE
  NimBLEDevice::stopAdvertising();
#else
  BLEDevice::getAdvertising()->stop();
#endif
  advertising = false;
}

bool teleconBleBegin() {
#if TELECON_SERIAL_INJECT
  serialLineBuffer.reserve(TELECON_LINE_RESERVE);
#endif
  lastRxLogLine = "";
  deviceConnected = false;
  prevDeviceConnected = false;
  notificationsEnabled = false;
  rcVehicleHandshakeReset();
  rcVehicleHandlersReset();
  teleconBinaryRxReset();

  pinMode(PIN_STATUS_LED, OUTPUT);
  digitalWrite(PIN_STATUS_LED, LOW);

#if TELECON_USE_NIMBLE
  NimBLEDevice::init(TELECON_BLE_NAME);
  NimBLEDevice::setMTU(BLE_REQUESTED_MTU);
  NimBLEDevice::setPower(ESP_PWR_LVL_N0);
  bleServer = NimBLEDevice::createServer();
  bleServer->setCallbacks(new ServerCallbacks());

  NimBLEService* service = bleServer->createService(NUS_SERVICE_UUID);
  NimBLECharacteristic* rx = service->createCharacteristic(
      NUS_RX_UUID,
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  rx->setCallbacks(new RxCallbacks());

  txCharacteristic = service->createCharacteristic(
      NUS_TX_UUID,
      NIMBLE_PROPERTY::NOTIFY);
  txCharacteristic->setCallbacks(new TxCallbacks());
  service->start();
#else
  BLEDevice::init(TELECON_BLE_NAME);
  BLEDevice::setMTU(BLE_REQUESTED_MTU);
  BLEDevice::setPower(ESP_PWR_LVL_N0);

  bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new ServerCallbacks());

  BLEService* service = bleServer->createService(NUS_SERVICE_UUID);
  BLECharacteristic* rx = service->createCharacteristic(
      NUS_RX_UUID,
      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  rx->setCallbacks(new RxCallbacks());

  txCharacteristic = service->createCharacteristic(
      NUS_TX_UUID,
      BLECharacteristic::PROPERTY_NOTIFY);
  txCharacteristic->addDescriptor(new BLE2902());
  service->start();
#endif

  teleconBleStartAdvertising();
  return true;
}

void teleconBlePoll() {
  if (!deviceConnected && prevDeviceConnected) {
    delay(80);
    teleconBleStartAdvertising();
    prevDeviceConnected = deviceConnected;
  }
  if (deviceConnected && !prevDeviceConnected) {
    prevDeviceConnected = deviceConnected;
  }
}

void teleconBleSerialPoll() {
#if TELECON_SERIAL_INJECT
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n') {
      if (serialLineBuffer.length() > 0) {
        serialLineBuffer.trim();
        if (serialLineBuffer.length() > 0) {
          rcVehicleHandleSerialInjectLine(serialLineBuffer);
        }
        serialLineBuffer = "";
      }
    } else if (c != '\r') {
      if (serialLineBuffer.length() < TELECON_MAX_LINE) {
        serialLineBuffer += c;
      } else {
        serialLineBuffer = "";
        DBG_PRINTLN("[SERIAL] line too long, discarded");
      }
    }
  }
#endif
}

bool teleconBleSendLine(const char* line) {
  if (line == nullptr) return false;
  size_t n = strlen(line);
  if (n + 1 >= TELECON_MAX_LINE) return false;

  uint8_t buf[TELECON_MAX_LINE];
  memcpy(buf, line, n);
  buf[n] = '\n';

  DBG_PRINT("[TX] ");
  DBG_PRINTLN(line);
  return bleNotifyBytes(buf, n + 1);
}

bool teleconBleSendBinary(const uint8_t* data, size_t len) {
  // No per-frame TX logging — keeps Serial focused on CTRL change lines.
  return bleNotifyBytes(data, len);
}

bool teleconBleIsConnected() {
  return deviceConnected;
}

bool teleconBleNotificationsEnabled() {
  return notificationsEnabled;
}

void teleconBleSetNotificationsEnabled(bool enabled) {
  notificationsEnabled = enabled;
}
