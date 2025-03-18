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
ESP_EVENT_DEFINE_BASE(USER_EVENTS);
static EventGroupHandle_t s_wifi_event_group;
static const int CONNECTED_BIT = BIT0;


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

void wifi_event_handle(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data)
{
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    ESP_LOGI(TAG, "Wifi starts finished"); 
    ESP_ERROR_CHECK(esp_wifi_connect());
  }
  else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *) event_data;
    ESP_LOGI(TAG, "Tried connected to %s", event->ssid);
    ESP_LOGI(TAG, "Wifi disconnected with error code %d, retrying...", event->reason);
    ESP_ERROR_CHECK(esp_wifi_connect());
    xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT);
  }
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t*) event_data;
    ESP_LOGI(TAG, "Got IPv4 event: " IPSTR, IP2STR(&event->ip_info.ip));
    xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT);
  }
  else if (event_base == USER_EVENTS && event_id == USER_CHANGE_WIFI) {
    wifi_config_t *wifi_config = (wifi_config_t*) event_data;
    if (xEventGroupGetBits(s_wifi_event_group) & CONNECTED_BIT) {
      ESP_ERROR_CHECK(esp_wifi_disconnect());
    }
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, wifi_config));
  }
}

esp_err_t connect_wifi(void) 
{
  s_wifi_event_group = xEventGroupCreate();
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

  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START, &wifi_event_handle, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &wifi_event_handle, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handle, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(USER_EVENTS, USER_CHANGE_WIFI, &wifi_event_handle, NULL));
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
  ESP_ERROR_CHECK(esp_event_post(USER_EVENTS, USER_CHANGE_WIFI, &wifi_config, sizeof(wifi_config_t), portMAX_DELAY));
  return ESP_OK;
}