#pragma once

/*
 * Shared SoftAP GET /camconfig — runtime framesize / JPEG quality / MJPEG FPS.
 *
 * Android (TeleCon4ESP32) Smooth / Balanced / High on the live camera screen:
 *   Smooth:   GET /camconfig?framesize=qqvga&quality=28&fps=8&ampdu_rx=0
 *   Balanced: GET /camconfig?framesize=vga&quality=15&fps=12
 *   High:     GET /camconfig?framesize=vga&quality=12&fps=0
 * Unknown keys ignored (not 400). ampdu_rx=0|1 is optional Xiaomi SoftAP
 * workaround (re-inits SoftAP; invalid value → 400).
 *
 * Default at boot (RC Vehicle one-board): QQVGA, quality 28, fps 8 (Smooth).
 *
 * This file lives in the sketch folder (Arduino compiles only that folder).
 * Arduino IDE compiles only the sketch folder — copy into each CAM sketch.
 *
 * Serial (TELECON_DEBUG 1, 115200): one boot hint + one GET/applied/FAIL line
 * per /camconfig. TELECON_DEBUG 0 compiles the logs out.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "esp_camera.h"

#ifndef CAMCONFIG_DEFAULT_FRAMESIZE
#define CAMCONFIG_DEFAULT_FRAMESIZE "qqvga"
#endif
#ifndef CAMCONFIG_DEFAULT_QUALITY
#define CAMCONFIG_DEFAULT_QUALITY 28
#endif
#ifndef CAMCONFIG_DEFAULT_FPS
#define CAMCONFIG_DEFAULT_FPS 8
#endif

/** WebServer subclass so /stream can accept a pending /camconfig client. */
class CameraStreamHttpServer : public WebServer {
 public:
  using WebServer::WebServer;
  bool hasPendingClient() { return _server.hasClient(); }
  NetworkClient takePendingClient() { return _server.accept(); }
};

void cameraStreamCamConfigInit(const char* framesizeToken = CAMCONFIG_DEFAULT_FRAMESIZE,
                               int quality = CAMCONFIG_DEFAULT_QUALITY,
                               int fps = CAMCONFIG_DEFAULT_FPS);

bool cameraStreamParseFramesize(const char* token, framesize_t* out);
const char* cameraStreamGetFramesizeToken();
framesize_t cameraStreamGetFramesize();
int cameraStreamGetQuality();
int cameraStreamGetFpsCap();
/** 0 = AMPDU RX off, 1 = on. */
int cameraStreamGetAmpduRx();
/** 0 = uncapped. */
uint32_t cameraStreamFpsPeriodMs();

/** SSID/pass for runtime ampdu_rx SoftAP re-init. pass=nullptr → open AP. */
void cameraStreamCamConfigSetSoftApCredentials(const char* ssid, const char* pass);
/** Sync AMPDU RX state after boot SoftAP (no Wi‑Fi re-init). */
void cameraStreamCamConfigNoteAmpduRx(bool enableRx);

void cameraStreamCamConfigNoteStreamActive(bool active);
/** True once after a framesize change applied while /stream was held. */
bool cameraStreamCamConfigConsumeEndStream();

/** Register GET /camconfig (other methods → 405). */
void cameraStreamCamConfigAttach(WebServer& server, bool& cameraReady);

/**
 * Handle /camconfig using WebServer args. Returns HTTP status after send.
 * GET with no query returns current params (200).
 */
int cameraStreamCamConfigSend(WebServer& server, bool cameraReady);

/** While /stream is blocked, steal a pending client and serve /camconfig. */
void cameraStreamCamConfigDrainPending(CameraStreamHttpServer& http, bool cameraReady);

/** Call each MJPEG iteration. Returns false if the stream should end. */
bool cameraStreamCamConfigPollWhileStreaming(CameraStreamHttpServer& http,
                                             bool cameraReady,
                                             bool clientConnected);
