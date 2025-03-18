#ifndef WIFI_CONNECT_H
#define WIFI_CONNECT_H

#include "esp_err.h"
#include "esp_event.h"

#define WIFI_SSID         "QUANGVINH"
#define WIFI_PWD          "01257564614"

#define MDNS_HOSTNAME      "remote"

ESP_EVENT_DECLARE_BASE(USER_EVENTS);

enum {
  USER_CHANGE_WIFI,
};

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t connect_wifi(void);
esp_err_t set_wifi(char *p_ssid, char *p_pwd);

#ifdef __cplusplus
}
#endif 

#endif