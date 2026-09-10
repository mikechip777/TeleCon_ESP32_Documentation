#pragma once

// Shared by CAM Wi‑Fi Simple .ino + CameraStream.cpp
// Set to 0 when SoftAP / Connect works and you no longer need Serial logs.
#ifndef ENABLE_DEBUG
#define ENABLE_DEBUG 1
#endif

#if ENABLE_DEBUG
  #define DBG_PRINT(x)      Serial.print(x)
  #define DBG_PRINTLN(x)    Serial.println(x)
  #define DBG_PRINTF(...)   Serial.printf(__VA_ARGS__)
#else
  #define DBG_PRINT(x)      ((void)0)
  #define DBG_PRINTLN(x)    ((void)0)
  #define DBG_PRINTF(...)   ((void)0)
#endif
