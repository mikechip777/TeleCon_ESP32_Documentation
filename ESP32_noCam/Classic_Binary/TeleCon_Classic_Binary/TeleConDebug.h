#pragma once

#include "TeleConConfig.h"

// Serial port needed for Monitor inject even when TELECON_DEBUG is off.
#if TELECON_DEBUG || TELECON_SERIAL_INJECT
  #define DBG_BEGIN(baud)   Serial.begin(baud)
#else
  #define DBG_BEGIN(baud)   ((void)0)
#endif

// Stick / HS / IO banners — only when TELECON_DEBUG is 1.
#if TELECON_DEBUG
  #define DBG_PRINT(x)      Serial.print(x)
  #define DBG_PRINTLN(x)    Serial.println(x)
  #define DBG_PRINTF(...)   Serial.printf(__VA_ARGS__)
#else
  #define DBG_PRINT(x)      ((void)0)
  #define DBG_PRINTLN(x)    ((void)0)
  #define DBG_PRINTF(...)   ((void)0)
#endif
