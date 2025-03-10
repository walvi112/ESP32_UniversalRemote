#ifndef WIFI_CONNECT_H
#define WIFI_CONNECT_H

#include "esp_err.h"

#define WIFI_SSID         ""
#define WIFI_PWD          ""
#define WIFI_MAX_RETRY     10

#define MDNS_HOSTNAME      "remote"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t connect_wifi(void);

#ifdef __cplusplus
}
#endif 

#endif