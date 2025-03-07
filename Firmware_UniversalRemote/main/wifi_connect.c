#include <string.h>
#include "wifi_connect.h"
#include "esp_log.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "sys/param.h"
#include "esp_netif.h"
#include "esp_wifi.h"

static const char *TAG = "WIFI";

esp_err_t connect_wifi(void) 
{
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
  
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  
  ESP_LOGI(TAG, "Starting wifi connection");
  esp_netif_create_default_wifi_ap();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  wifi_config_t wifi_config = {
    .ap = {
      .ssid = WIFI_SSID,
      .ssid_len = strlen(WIFI_SSID),
      .channel = WIFI_CHANNEL,
      .password = WIFI_PWD,
      .max_connection = 4,
      .authmode = WIFI_AUTH_WPA2_PSK,
      .pmf_cfg = {
        .required = true},
    }
  };

  if (strlen(WIFI_PWD) == 0) {
    wifi_config.ap.authmode = WIFI_AUTH_OPEN;
  }

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d", WIFI_SSID, WIFI_PWD, WIFI_CHANNEL);

  return ESP_OK;
}
