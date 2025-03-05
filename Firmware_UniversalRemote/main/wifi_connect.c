#include "wifi_connect.h"
#include "esp_log.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "sys/param.h"
#include "esp_netif.h"
#include "esp_wifi.h"

static const char *TAG = "HTTP-SERVER";
static SemaphoreHandle_t get_ip_semp;
static int wifi_retry = 0;

esp_err_t disconnect_wifi(void) 
{
  if (get_ip_semp) {
    vSemaphoreDelete(get_ip_semp);
  }
  return esp_wifi_disconnect();
}

void wifi_start_handle(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data)
{
  wifi_retry = 0;
  ESP_LOGI(TAG, "Wifi starts finished"); 
  ESP_ERROR_CHECK(esp_wifi_connect());
}

void wifi_disconnected_handle(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data) 
{
  if (wifi_retry < WIFI_MAX_RETRY) {
    wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *) event_data;

    ESP_LOGI(TAG, "Tried connected to %s", event->ssid);
    ESP_LOGI(TAG, "Wifi disconnected with error code %d, retrying...", event->reason);
    ESP_ERROR_CHECK(esp_wifi_connect());
    wifi_retry++;
    return;
  }
  if (get_ip_semp) {
    xSemaphoreGive(get_ip_semp);
    disconnect_wifi();
    return;  
  }
}

void sta_got_ip_handle(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data)
{
  wifi_retry = 0;
  ip_event_got_ip_t *event = (ip_event_got_ip_t*) event_data;
  ESP_LOGI(TAG, "Got IPv4 event: " IPSTR, IP2STR(&event->ip_info.ip));
  if (get_ip_semp) {
    xSemaphoreGive(get_ip_semp);
  }
}

esp_err_t connect_wifi(void) 
{
  ESP_LOGI(TAG, "Starting wifi connection");
  esp_netif_create_default_wifi_sta();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  wifi_config_t wifi_config = {
    .sta = {
      .ssid = WIFI_SSID,
      .password = WIFI_PWD,
      .scan_method = WIFI_ALL_CHANNEL_SCAN,
      .sort_method = WIFI_CONNECT_AP_BY_SECURITY,
      .threshold 
        .authmode = WIFI_AUTH_OPEN,
    }
  };
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

  get_ip_semp = xSemaphoreCreateBinary();
  if (get_ip_semp == NULL)
    return ESP_ERR_NO_MEM;
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START, &wifi_start_handle, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &wifi_disconnected_handle, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &sta_got_ip_handle, NULL));
  
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(TAG, "Waiting for IP");
  xSemaphoreTake(get_ip_semp, portMAX_DELAY);

  if (wifi_retry >= WIFI_MAX_RETRY) {
    return ESP_FAIL;
  }
  return ESP_OK;
}
