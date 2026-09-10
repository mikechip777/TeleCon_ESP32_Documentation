#include "SteerCenter.h"

#include <Preferences.h>

namespace {

Preferences prefs;

int centerUs = STEER_CENTER_US_DEFAULT;
int travelUs = STEER_TRAVEL_US_DEFAULT;
int bias0 = 0;
int maxBias = STEER_BIAS_MAX_DEFAULT;
bool pendingSave = false;

/** Last PWM/mix actually applied — used when locking center (SET and/or BB66). */
int lastAppliedUs = STEER_CENTER_US_DEFAULT;
int lastAppliedBias = 0;

/** True after a save until the next live CTRL apply — blocks double-shift (SET+BB66). */
bool saveLatched = false;

constexpr const char* NVS_NS = "steer_ctr";
constexpr const char* KEY_CENTER = "centerUs";
constexpr const char* KEY_BIAS = "bias0";

String getCsvValue(const String& line, const char* key) {
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

void noteAppliedInternal(int rxNeg100to100) {
  int s = constrain(rxNeg100to100, -100, 100);
  lastAppliedUs = (int)map(s, -100, 100, centerUs - travelUs, centerUs + travelUs);
  lastAppliedBias = (int)map(s, -100, 100, bias0 - maxBias, bias0 + maxBias);
}

}  // namespace

void steerCenterBegin() {
  travelUs = STEER_TRAVEL_US_DEFAULT;
  maxBias = STEER_BIAS_MAX_DEFAULT;

  if (prefs.begin(NVS_NS, true)) {
    centerUs = prefs.getInt(KEY_CENTER, STEER_CENTER_US_DEFAULT);
    bias0 = prefs.getInt(KEY_BIAS, 0);
    prefs.end();
  } else {
    centerUs = STEER_CENTER_US_DEFAULT;
    bias0 = 0;
  }

  centerUs = constrain(centerUs, 500, 2500);
  bias0 = constrain(bias0, -maxBias, maxBias);
  lastAppliedUs = centerUs;
  lastAppliedBias = bias0;
  pendingSave = false;
  saveLatched = false;
}

void steerCenterLoop() {
  if (!pendingSave) return;
  pendingSave = false;

  if (!prefs.begin(NVS_NS, false)) return;
  prefs.putInt(KEY_CENTER, centerUs);
  prefs.putInt(KEY_BIAS, bias0);
  prefs.end();
}

int steerCenterMapUs(int rxNeg100to100) {
  int s = constrain(rxNeg100to100, -100, 100);
  int lo = centerUs - travelUs;
  int hi = centerUs + travelUs;
  return (int)map(s, -100, 100, lo, hi);
}

int steerCenterMapBias(int rxNeg100to100) {
  int s = constrain(rxNeg100to100, -100, 100);
  int lo = bias0 - maxBias;
  int hi = bias0 + maxBias;
  return (int)map(s, -100, 100, lo, hi);
}

uint32_t steerCenterDuty12FromUs(int us) {
  int pulse = constrain(us, 500, 2500);
  // 50 Hz period = 20000 µs; 12-bit LEDC full scale = 4095
  return (uint32_t)((long)pulse * 4095L / 20000L);
}

uint32_t steerCenterDuty12FromRx(int rxNeg100to100) {
  steerCenterNoteApplied(rxNeg100to100);
  return steerCenterDuty12FromUs(lastAppliedUs);
}

void steerCenterNoteApplied(int rxNeg100to100) {
  noteAppliedInternal(rxNeg100to100);
  saveLatched = false;
}

bool steerCenterSaveFromCurrentRx(int currentRxNeg100to100) {
  if (!saveLatched) {
    // First of a SET/BB66 pair: freeze output for this rx under the pre-save map.
    noteAppliedInternal(currentRxNeg100to100);
  }
  centerUs = constrain(lastAppliedUs, 500, 2500);
  bias0 = constrain(lastAppliedBias, -maxBias, maxBias);
  lastAppliedUs = centerUs;
  lastAppliedBias = bias0;
  pendingSave = true;
  saveLatched = true;
  return true;
}

bool steerCenterHandleSetLine(const String& line, int fallbackRx) {
  String flag = getCsvValue(line, "steer_center");
  if (flag.length() == 0) return false;
  if (flag.toInt() != 1) return false;

  int rx = fallbackRx;
  String rxStr = getCsvValue(line, "rx");
  if (rxStr.length() > 0) {
    rx = constrain(rxStr.toInt(), -100, 100);
  }
  return steerCenterSaveFromCurrentRx(rx);
}

int steerCenterGetUs() {
  return centerUs;
}

int steerCenterGetBias0() {
  return bias0;
}
