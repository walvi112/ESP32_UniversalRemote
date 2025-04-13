#include <string.h>
#include "wifi_connect.h"
#include "esp_log.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "sys/param.h"
#include "esp_netif.h"
#include "mdns.h"

static const char *TAG = "WIFI";
static const char *TAG_STA = "WIFI Sta";
static const char *TAG_AP = "WIFI Ap";

ESP_EVENT_DEFINE_BASE(USER_EVENTS);
static EventGroupHandle_t s_wifi_event_group;
static const int S_CONNECTED_BIT = BIT0;

static nvs_handle_t s_wifi_nvs_handle;
static wifi_mode_t s_wifi_mode;

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

static esp_err_t wifi_sta_init(void)
{
  ESP_LOGI(TAG_STA, "Init wifi station");

  wifi_config_t wifi_config_sta = {
    .sta = {
      .ssid = "default",
      .password = "1234567890",
      .sort_method = WIFI_CONNECT_AP_BY_SECURITY,
      .threshold 
        .authmode = WIFI_AUTH_OPEN,
    }
  };

  size_t length = 0;
  esp_err_t err;

  ESP_ERROR_CHECK(nvs_open(WIFI_NAMESPACE, NVS_READWRITE, &s_wifi_nvs_handle));
  err = nvs_get_str(s_wifi_nvs_handle, WIFI_SSID_KEY, NULL, &length);
  if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return ESP_FAIL;
  if (length == 0) {
    nvs_set_str(s_wifi_nvs_handle, WIFI_SSID_KEY, "default");
    ESP_ERROR_CHECK(nvs_commit(s_wifi_nvs_handle));
  } else {
    ESP_ERROR_CHECK(nvs_get_str(s_wifi_nvs_handle, WIFI_SSID_KEY, (char*) &wifi_config_sta.sta.ssid, &length));
  }

  err = nvs_get_str(s_wifi_nvs_handle, WIFI_PWD_KEY, NULL, &length);
  if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return ESP_FAIL;
  if (length == 0) {
    nvs_set_str(s_wifi_nvs_handle, WIFI_PWD_KEY, "1234567890");
    ESP_ERROR_CHECK(nvs_commit(s_wifi_nvs_handle));
  } else {
    ESP_ERROR_CHECK(nvs_get_str(s_wifi_nvs_handle, WIFI_PWD_KEY, (char*) &wifi_config_sta.sta.password, &length));
  }
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config_sta));
  ESP_ERROR_CHECK(esp_wifi_start());
  esp_wifi_get_mode(&s_wifi_mode);
  return ESP_OK;
}

static esp_err_t wifi_ap_init(void) 
{
  ESP_LOGI(TAG_AP, "Init wifi ap");
  wifi_config_t wifi_config_ap = {
    .ap = {
      .ssid = WIFI_AP_SSID,
      .password = WIFI_AP_PWD,
      .channel = 1,
      .max_connection = 1,
      .authmode = WIFI_AUTH_WPA2_PSK,
      .pmf_cfg = {
        .required = false,
      },
    },
  };

  if (strlen(WIFI_AP_PWD) == 0) {
    wifi_config_ap.ap.authmode = WIFI_AUTH_OPEN;
  }

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config_ap));
  ESP_ERROR_CHECK(esp_wifi_start());
  esp_wifi_get_mode(&s_wifi_mode);
  return ESP_OK;
}

static void wifi_event_handle(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data)
{
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    ESP_LOGI(TAG, "Wifi starts finished"); 
    esp_wifi_connect();
  } 
  else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *) event_data;
    ESP_LOGI(TAG_STA, "Tried connected to %s", event->ssid);
    ESP_LOGI(TAG_STA, "Wifi disconnected with error code %d, retrying...", event->reason);
    esp_wifi_connect();
    xEventGroupClearBits(s_wifi_event_group, S_CONNECTED_BIT);
  } 
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t*) event_data;
    ESP_LOGI(TAG_STA, "Got IPv4 event: " IPSTR, IP2STR(&event->ip_info.ip));
    xEventGroupSetBits(s_wifi_event_group, S_CONNECTED_BIT);
  } 
  else if (event_base == USER_EVENTS && event_id == USER_CHANGE_WIFI) {
    if (s_wifi_mode == WIFI_MODE_AP) {
      ESP_ERROR_CHECK(esp_wifi_stop());
      ESP_ERROR_CHECK(wifi_sta_init());
    }
    wifi_config_t *wifi_config = (wifi_config_t*) event_data;
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, wifi_config));
    esp_wifi_connect();
  } 
  else if (event_base == USER_EVENTS && event_id == USER_WIFI_BTN) {
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    xEventGroupClearBits(s_wifi_event_group, S_CONNECTED_BIT);
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(wifi_ap_init());
  } 
}

esp_err_t wifi_init(void) 
{
  s_wifi_event_group = xEventGroupCreate();
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  ESP_LOGI(TAG, "Starting mDNS service");
  initialise_mdns();

  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START, &wifi_event_handle, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &wifi_event_handle, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handle, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(USER_EVENTS, ESP_EVENT_ANY_ID, &wifi_event_handle, NULL));
  
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  esp_netif_t *esp_netif_sta = esp_netif_create_default_wifi_sta();
  esp_netif_t *esp_netif_ap = esp_netif_create_default_wifi_ap();
  ESP_ERROR_CHECK(wifi_sta_init());
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
  ESP_ERROR_CHECK(nvs_set_str(s_wifi_nvs_handle, WIFI_SSID_KEY, p_ssid));
  ESP_ERROR_CHECK(nvs_set_str(s_wifi_nvs_handle, WIFI_PWD_KEY, p_pwd));
  ESP_ERROR_CHECK(nvs_commit(s_wifi_nvs_handle));
  return ESP_OK;
}

esp_err_t reset_wifi()
{
  ESP_LOGI(TAG, "Reset wifi");
  if (s_wifi_mode == WIFI_MODE_STA) {
    ESP_ERROR_CHECK(esp_event_post(USER_EVENTS, USER_WIFI_BTN, NULL, 0, portMAX_DELAY));
  }
  return ESP_OK;
}

wifi_mode_t get_wifi_mode()
{
  return s_wifi_mode;
}