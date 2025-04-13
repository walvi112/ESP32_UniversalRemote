#ifndef WIFI_CONNECT_H
#define WIFI_CONNECT_H

#include "esp_err.h"
#include "esp_event.h"
#include "esp_wifi.h"

#define WIFI_NAMESPACE            "wifi_storage"

#define WIFI_SSID_KEY             "wifi_ssid"
#define WIFI_PWD_KEY              "wifi_password"

#define WIFI_AP_SSID              "esp32remote"
#define WIFI_AP_PWD               ""

#define MAX_WIFI_SSID_LENGTH      MAX_SSID_LEN
#define MAX_WIFI_PWD_LENGTH       MAX_PASSPHRASE_LEN

#define MDNS_HOSTNAME             "remote"

ESP_EVENT_DECLARE_BASE(USER_EVENTS);

enum {
  USER_CHANGE_WIFI,
  USER_WIFI_BTN,
};

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t wifi_init(void);
esp_err_t set_wifi(char *p_ssid, char *p_pwd);
esp_err_t reset_wifi();
wifi_mode_t get_wifi_mode(); 

#ifdef __cplusplus
}
#endif 

#endif