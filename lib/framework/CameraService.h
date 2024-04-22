#ifndef CameraService_h
#define CameraService_h

/**
 *   ESP32 SvelteKit
 *
 *   A simple, secure and extensible framework for IoT projects for ESP32 platforms
 *   with responsive Sveltekit front-end built with TailwindCSS and DaisyUI.
 *   https://github.com/theelims/ESP32-sveltekit
 *
 *   Copyright (C) 2018 - 2023 rjwats
 *   Copyright (C) 2023 theelims
 *
 *   All Rights Reserved. This software may be modified and distributed under
 *   the terms of the LGPL v3 license. See the LICENSE file for details.
 **/

#define CAMERA_MODEL_AI_THINKER

#include <ArduinoJson.h>
#include <CameraPins.h>
#include <PsychicHttp.h>
#include <SecurityManager.h>
#include <WiFi.h>
#include <esp_camera.h>

#define STREAM_SERVICE_PATH "/rest/camera/stream"
#define STILL_SERVICE_PATH "/rest/camera/still"

#define PART_BOUNDARY "frame"

camera_fb_t *safe_camera_fb_get();

class CameraService
{
  public:
    CameraService(PsychicHttpServer *server, SecurityManager *securityManager);

    void begin();
    void cameraStreamTask();

  private:
    PsychicHttpServer *_server;
    SecurityManager *_securityManager;
    PsychicStream _videoStream;
    esp_err_t cameraStill(PsychicRequest *request);
    // esp_err_t cameraStream(PsychicRequest *request);
    esp_err_t InitializeCamera();
};

#endif // end CameraService_h
