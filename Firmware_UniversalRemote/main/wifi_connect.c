#include <string.h>
#include "wifi_connect.h"
#include "esp_log.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "sys/param.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "mdns.h"

static const char *TAG = "WIFI";
static int wifi_retry = 0;

static void initialise_mdns(void)
{
  #ifndef MDNS_HOSTNAME
    char *hostname = "espremote";
  #else
    char *hostname = MDNS_HOSTNAME;
  #endif

    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set(hostname));
    ESP_LOGI(TAG, "mDNS hostname set to: [%s]", hostname);
    ESP_ERROR_CHECK(mdns_instance_name_set("esp_mdns_server"));
    ESP_ERROR_CHECK(mdns_service_add("UniversalRemote-WebServer", "_http", "_tcp", 80, NULL, 0));
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
  wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *) event_data;

    ESP_LOGI(TAG, "Tried connected to %s", event->ssid);
    ESP_LOGI(TAG, "Wifi disconnected with error code %d, retrying...", event->reason);
    ESP_ERROR_CHECK(esp_wifi_connect());

    return;
}

void sta_got_ip_handle(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data)
{
  wifi_retry = 0;
  ip_event_got_ip_t *event = (ip_event_got_ip_t*) event_data;
  ESP_LOGI(TAG, "Got IPv4 event: " IPSTR, IP2STR(&event->ip_info.ip));
}

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

  ESP_LOGI(TAG, "Starting mDNS service");
  initialise_mdns();

  ESP_LOGI(TAG, "Starting wifi connection");
  esp_netif_create_default_wifi_sta();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  wifi_config_t wifi_config = {
    .sta = {
      .ssid = WIFI_SSID,
      .password = WIFI_PWD,
      .sort_method = WIFI_CONNECT_AP_BY_SECURITY,
      .threshold 
        .authmode = WIFI_AUTH_OPEN,
    }
  };
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START, &wifi_start_handle, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &wifi_disconnected_handle, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &sta_got_ip_handle, NULL));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_LOGI(TAG, "Waiting for IP");

  return ESP_OK;
}

esp_err_t set_wifi(char *p_ssid, char *p_pwd)
{
  wifi_config_t wifi_config = {
    .sta = {
      .sort_method = WIFI_CONNECT_AP_BY_SECURITY,
      .threshold 
        .authmode = WIFI_AUTH_OPEN,
    }
  };
  memcpy(wifi_config.sta.ssid, p_ssid, sizeof(wifi_config.sta.ssid) - 1);
  memcpy(wifi_config.sta.password, p_pwd, sizeof(wifi_config.sta.password) - 1);
  ESP_ERROR_CHECK(esp_wifi_disconnect());
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  return ESP_OK;
}