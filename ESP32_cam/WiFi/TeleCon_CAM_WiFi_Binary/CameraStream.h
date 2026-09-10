#pragma once

#include <Arduino.h>

/** Optional hook invoked while a client is attached to GET /stream (keep TCP RC alive). */
typedef void (*CameraStreamIdleHook)();

void cameraStreamSetIdleHook(CameraStreamIdleHook hook);
void cameraStreamBegin();
void cameraStreamPoll();
bool cameraStreamReady();
/** True while GET /stream holds the HTTP client (TCP RC must stay cooperative). */
bool cameraStreamIsActive();
