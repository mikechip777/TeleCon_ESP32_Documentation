#pragma once

#include <Arduino.h>

/** Optional idle hook while a client is attached to GET /stream. */
typedef void (*CameraStreamIdleHook)();

void cameraStreamSetIdleHook(CameraStreamIdleHook hook);
/** SoftAP + camera + HTTP /stream /capture /camconfig (no SoftAP TCP control). */
void cameraStreamBegin(const char* apSsid, const char* apPass);
void cameraStreamPoll();
bool cameraStreamReady();
