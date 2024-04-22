#include <CameraService.h>

static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

SemaphoreHandle_t cameraMutex = xSemaphoreCreateMutex();

void cameraStreamTask(void *param);
void startCameraStream(void *param);

camera_fb_t *safe_camera_fb_get()
{
    camera_fb_t *fb = NULL;
    if (xSemaphoreTake(cameraMutex, portMAX_DELAY))
    {
        fb = esp_camera_fb_get();
        xSemaphoreGive(cameraMutex);
    }
    return fb;
}

void sendStreamData(PsychicStreamClient *client)
{
    ESP_LOGI("Stream callback", "Stream client connected");

    // camera_fb_t *fb = NULL;
    // esp_err_t res = ESP_OK;
    // size_t _jpg_buf_len = 0;
    // uint8_t *_jpg_buf = NULL;
    // char *part_buf[64];
    // int64_t fr_start = esp_timer_get_time();

    // res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    // if (res != ESP_OK)
    // {
    //     return vTaskDelete(NULL);
    // }
    // while (true)
    // {
    //     fb = safe_camera_fb_get();
    //     if (!fb)
    //     {
    //         Serial.println("Camera capture failed");
    //         res = ESP_FAIL;
    //     }
    //     else
    //     {
    //         if (fb->width > 400)
    //         {
    //             if (fb->format != PIXFORMAT_JPEG)
    //             {
    //                 bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
    //                 esp_camera_fb_return(fb);
    //                 fb = NULL;
    //                 if (!jpeg_converted)
    //                 {
    //                     Serial.println("JPEG compression failed");
    //                     res = ESP_FAIL;
    //                 }
    //             }
    //             else
    //             {
    //                 _jpg_buf_len = fb->len;
    //                 _jpg_buf = fb->buf;
    //             }
    //         }
    //     }
    //     if (res == ESP_OK)
    //     {
    //         size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
    //         res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    //     }
    //     if (res == ESP_OK)
    //     {
    //         res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    //     }
    //     if (res == ESP_OK)
    //     {
    //         res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    //     }
    //     if (fb)
    //     {
    //         esp_camera_fb_return(fb);
    //         fb = NULL;
    //         _jpg_buf = NULL;
    //     }
    //     else if (_jpg_buf)
    //     {
    //         free(_jpg_buf);
    //         _jpg_buf = NULL;
    //     }
    //     if (res != ESP_OK)
    //     {
    //         break;
    //     }
    //     taskYIELD();
    //     int64_t fr_end = esp_timer_get_time();
    //     int64_t frame_time = fr_end - fr_start;
    //     int64_t delay = 30000ll - frame_time; // 30ms per frame
    //     if (delay > 0)
    //         vTaskDelay(pdMS_TO_TICKS(delay));
    //     // Serial.printf("MJPG: %uB\n",(uint32_t)(_jpg_buf_len));
    // }
}

CameraService::CameraService(PsychicHttpServer *server, SecurityManager *securityManager)
    : _server(server), _securityManager(securityManager), _videoStream(_STREAM_CONTENT_TYPE)
{
}
void CameraService::begin()
{
    InitializeCamera();
    _server->on(STILL_SERVICE_PATH, HTTP_GET,
                _securityManager->wrapRequest(std::bind(&CameraService::cameraStill, this, std::placeholders::_1),
                                              AuthenticationPredicates::IS_AUTHENTICATED));
    // _server->on(STREAM_SERVICE_PATH, HTTP_GET,
    //             _securityManager->wrapRequest(std::bind(&CameraService::cameraStream, this, std::placeholders::_1),
    //                                           AuthenticationPredicates::IS_AUTHENTICATED));

    _server->on(STREAM_SERVICE_PATH, HTTP_GET, &_videoStream);

    _videoStream.onOpen(sendStreamData);

    // _videoStream.onRequest(cameraStreamTask);

    _videoStream.onClose(
        [](PsychicStreamClient *client) { ESP_LOGI("Stream callback", "Stream client disconnected"); });

    ESP_LOGV("CameraService", "Registered GET endpoint: %s", STILL_SERVICE_PATH);

    xTaskCreatePinnedToCore(startCameraStream, "startCameraStream", 4096, this, 5, NULL, 1);
}

esp_err_t CameraService::InitializeCamera()
{
    camera_config_t camera_config;
    camera_config.ledc_channel = LEDC_CHANNEL_0;
    camera_config.ledc_timer = LEDC_TIMER_0;
    camera_config.pin_d0 = Y2_GPIO_NUM;
    camera_config.pin_d1 = Y3_GPIO_NUM;
    camera_config.pin_d2 = Y4_GPIO_NUM;
    camera_config.pin_d3 = Y5_GPIO_NUM;
    camera_config.pin_d4 = Y6_GPIO_NUM;
    camera_config.pin_d5 = Y7_GPIO_NUM;
    camera_config.pin_d6 = Y8_GPIO_NUM;
    camera_config.pin_d7 = Y9_GPIO_NUM;
    camera_config.pin_xclk = XCLK_GPIO_NUM;
    camera_config.pin_pclk = PCLK_GPIO_NUM;
    camera_config.pin_vsync = VSYNC_GPIO_NUM;
    camera_config.pin_href = HREF_GPIO_NUM;
    camera_config.pin_sccb_sda = SIOD_GPIO_NUM;
    camera_config.pin_sccb_scl = SIOC_GPIO_NUM;
    camera_config.pin_pwdn = PWDN_GPIO_NUM;
    camera_config.pin_reset = RESET_GPIO_NUM;
    camera_config.xclk_freq_hz = 20000000;
    camera_config.pixel_format = PIXFORMAT_JPEG;

    if (psramFound())
    {
        camera_config.frame_size = FRAMESIZE_SVGA;
        camera_config.jpeg_quality = 10;
        camera_config.fb_count = 2;
    }
    else
    {
        camera_config.frame_size = FRAMESIZE_SVGA;
        camera_config.jpeg_quality = 12;
        camera_config.fb_count = 1;
    }

    log_i("Initializing camera");
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
        log_e("Camera probe failed with error 0x%x", err);

    return err;
}

esp_err_t CameraService::cameraStill(PsychicRequest *request)
{
    camera_fb_t *fb = safe_camera_fb_get();
    if (!fb)
    {
        Serial.println("Camera capture failed");
        request->reply(500, "text/plain", "Camera capture failed");
        return ESP_FAIL;
    }
    PsychicStreamResponse response = PsychicStreamResponse(request, "image/jpeg", "capture.jpg");
    response.beginSend();
    response.write(fb->buf, fb->len);
    esp_camera_fb_return(fb);
    return response.endSend();
}

void startCameraStream(void *_this)
{
    static_cast<CameraService *>(_this)->cameraStreamTask();
    // xTaskCreatePinnedToCore(cameraStreamTask, "cameraStreamTask", 4096, param, 5, NULL, 1);
}

void CameraService::cameraStreamTask()
{
    ESP_LOGV("CameraService", "Starting thread");

    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t *_jpg_buf = NULL;
    char *part_buf[64];
    int64_t fr_start = esp_timer_get_time();

    while (true)
    {
        fb = safe_camera_fb_get();
        if (!fb)
        {
            Serial.println("Camera capture failed");
            res = ESP_FAIL;
        }
        else
        {
            if (fb->width > 400)
            {
                if (fb->format != PIXFORMAT_JPEG)
                {
                    bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
                    esp_camera_fb_return(fb);
                    fb = NULL;
                    if (!jpeg_converted)
                    {
                        Serial.println("JPEG compression failed");
                        res = ESP_FAIL;
                    }
                }
                else
                {
                    _jpg_buf_len = fb->len;
                    _jpg_buf = fb->buf;
                }
            }
        }
            size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
            _videoStream.write((const uint8_t *)part_buf, hlen);
            _videoStream.write(_jpg_buf, _jpg_buf_len);
            _videoStream.write((const uint8_t *)_STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
            if (fb)
            {
                esp_camera_fb_return(fb);
                fb = NULL;
                _jpg_buf = NULL;
            }
        else if (_jpg_buf)
        {
            free(_jpg_buf);
            _jpg_buf = NULL;
        }
        if (res != ESP_OK)
        {
            break;
        }
        taskYIELD();
        int64_t fr_end = esp_timer_get_time();
        int64_t frame_time = fr_end - fr_start;
        int64_t delay = 30000ll - frame_time; // 30ms per frame
        if (delay > 0)
            vTaskDelay(pdMS_TO_TICKS(delay));
        // Serial.printf("MJPG: %uB\n",(uint32_t)(_jpg_buf_len));
    }
    vTaskDelete(NULL);
}

// esp_err_t CameraService::cameraStream(PsychicRequest *request)
// {
//     httpd_req_t *req = request->request();
//     BaseType_t res = xTaskCreate(cameraStreamTask, "cameraStreamTask", 4096, req, 5, NULL);
//     if (res != pdPASS)
//     {
//         request->reply(500, "text/plain", "Camera stream failed");
//         return ESP_FAIL;
//     }
//     for (int i = 0; i < 10; i++)
//     {
//         log_i("Waiting for camera stream");
//         taskYIELD();
//         vTaskDelay(1000 / portTICK_PERIOD_MS);
//     }
//     return ESP_OK;
// }
