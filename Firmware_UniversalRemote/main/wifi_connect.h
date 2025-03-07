#ifndef WIFI_CONNECT_H
#define WIFI_CONNECT_H

#include "esp_err.h"


#define WIFI_SSID         "ESP_WIFI"
#define WIFI_PWD          "1234567890"
#define WIFI_CHANNEL       1

#define WIFI_MAX_RETRY     10

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t connect_wifi(void);

#ifdef __cplusplus
}
#endif 

#endif