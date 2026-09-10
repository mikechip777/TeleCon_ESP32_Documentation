#pragma once

#include <Arduino.h>

/** Optional hook while a client is attached to GET /stream (keep TCP RC alive). */
typedef void (*CameraStreamIdleHook)();

void cameraStreamSetIdleHook(CameraStreamIdleHook hook);
/** SoftAP + camera + HTTP /stream /capture. Call before TCP server begin. */
void cameraStreamBegin(const char* apSsid, const char* apPass);
void cameraStreamPoll();
bool cameraStreamReady();
/** True while GET /stream holds the HTTP client (TCP RC must stay cooperative). */
bool cameraStreamIsActive();
