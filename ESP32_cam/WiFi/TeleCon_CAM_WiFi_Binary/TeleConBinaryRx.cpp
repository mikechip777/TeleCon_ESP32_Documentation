/*
 * Demux TCP bytes into text lines, AA 55 RC control, or BB 66 button packets.
 * Same logic as Classic/BLE demux — only the transport differs.
 */

#include "TeleConBinaryRx.h"
#include "TeleConConfig.h"
#include "TeleConWifi.h"
#include "RcVehicleHandlers.h"

namespace {

enum RxMode : uint8_t {
  RX_IDLE = 0,
  RX_LINE,
  RX_RC,
  RX_BTN,
};

RxMode rxMode = RX_IDLE;
String lineBuffer;
uint8_t binaryBuf[TELECON_RC_PACKET_SIZE];
size_t binaryPos = 0;

bool isPrintableAscii(uint8_t b) {
  return b >= 32 && b <= 126;
}

}  // namespace

void teleconBinaryRxReset() {
  rxMode = RX_IDLE;
  binaryPos = 0;
  lineBuffer = "";
}

bool teleconBinaryRxFeed(uint8_t byte) {
  switch (rxMode) {
    case RX_IDLE:
      if (byte == TELECON_RC_HDR0) {
        binaryBuf[0] = byte;
        binaryPos = 1;
        rxMode = RX_RC;
      } else if (byte == TELECON_BTN_HDR0) {
        binaryBuf[0] = byte;
        binaryPos = 1;
        rxMode = RX_BTN;
      } else if (byte == '\r') {
        // ignore
      } else if (byte == '\n') {
        if (lineBuffer.length() > 0) {
          onLineReceived(lineBuffer);
          lineBuffer = "";
        }
      } else if (isPrintableAscii(byte)) {
        lineBuffer = (char)byte;
        rxMode = RX_LINE;
      }
      break;

    case RX_LINE:
      if (byte == '\n') {
        if (lineBuffer.length() > 0) {
          onLineReceived(lineBuffer);
        }
        lineBuffer = "";
        rxMode = RX_IDLE;
      } else if (byte != '\r') {
        if (lineBuffer.length() < TELECON_MAX_LINE) {
          lineBuffer += (char)byte;
        } else {
          lineBuffer = "";
          rxMode = RX_IDLE;
          DBG_PRINTLN("[RX] line too long, discarded");
        }
      }
      break;

    case RX_RC:
      binaryBuf[binaryPos++] = byte;
      if (binaryPos == 2 && binaryBuf[1] != TELECON_RC_HDR1) {
        uint8_t retry = byte;
        teleconBinaryRxReset();
        teleconBinaryRxFeed(retry);
        break;
      }
      if (binaryPos >= TELECON_RC_PACKET_SIZE) {
        handleRcBinaryPacket(binaryBuf, TELECON_RC_PACKET_SIZE);
        binaryPos = 0;
        rxMode = RX_IDLE;
      }
      break;

    case RX_BTN:
      binaryBuf[binaryPos++] = byte;
      if (binaryPos == 2 && binaryBuf[1] != TELECON_BTN_HDR1) {
        uint8_t retry = byte;
        teleconBinaryRxReset();
        teleconBinaryRxFeed(retry);
        break;
      }
      if (binaryPos >= TELECON_BTN_PACKET_SIZE) {
        handleRcBtnBinary(binaryBuf, TELECON_BTN_PACKET_SIZE);
        binaryPos = 0;
        rxMode = RX_IDLE;
      }
      break;
  }
  return true;
}
