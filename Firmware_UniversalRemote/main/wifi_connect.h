#ifndef WIFI_CONNECT_H
#define WIFI_CONNECT_H

#include "esp_err.h"

#define WIFI_SSID         "DEFAULT"
#define WIFI_PWD          "DEFAULT"

#define MDNS_HOSTNAME      "remote"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t connect_wifi(void);
esp_err_t set_wifi(char *p_ssid, char *p_pwd);

#ifdef __cplusplus
}
#endif 

#endif