#include <string.h>
#include "wifi_connect.h"
#include "esp_log.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "sys/param.h"
#include "esp_netif.h"
#include "mdns.h"
#include "esp_smartconfig.h"

static const char *TAG = "WIFI";

ESP_EVENT_DEFINE_BASE(USER_EVENTS);
static EventGroupHandle_t s_wifi_event_group;
static const int S_CONNECTED_BIT = BIT0;
static const int S_ESPTOUCH_START_BIT = BIT1;
static const int S_ESPTOUCH_DONE_BIT = BIT2;


static nvs_handle_t s_wifi_nvs_handle;

static void smartconfig_task(void *args)
{
  EventBits_t uxBits;
  xEventGroupSetBits(s_wifi_event_group, S_ESPTOUCH_START_BIT);
  ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH_V2));
  smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_esptouch_set_timeout(30));
  ESP_ERROR_CHECK(esp_smartconfig_start(&cfg));
  while (1) {
      uxBits = xEventGroupWaitBits(s_wifi_event_group, S_CONNECTED_BIT | S_ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY);
      if(uxBits & S_CONNECTED_BIT) {
          ESP_LOGI(TAG, "WiFi Connected to ap");
      }
      if(uxBits & S_ESPTOUCH_DONE_BIT) {
          ESP_LOGI(TAG, "smartconfig over");
          esp_smartconfig_stop();
          xEventGroupClearBits(s_wifi_event_group, S_ESPTOUCH_START_BIT | S_ESPTOUCH_DONE_BIT);
          vTaskDelete(NULL);
      }
  }
}

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
    if (xEventGroupGetBits(s_wifi_event_group) & S_ESPTOUCH_START_BIT)  return;
    wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *) event_data;
    ESP_LOGI(TAG, "Tried connected to %s", event->ssid);
    ESP_LOGI(TAG, "Wifi disconnected with error code %d, retrying...", event->reason);
    esp_wifi_connect();
    xEventGroupClearBits(s_wifi_event_group, S_CONNECTED_BIT);
  } 
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t*) event_data;
    ESP_LOGI(TAG, "Got IPv4 event: " IPSTR, IP2STR(&event->ip_info.ip));
    xEventGroupSetBits(s_wifi_event_group, S_CONNECTED_BIT);
  } 
  else if (event_base == USER_EVENTS && event_id == USER_CHANGE_WIFI) {
    wifi_config_t *wifi_config = (wifi_config_t*) event_data;
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, wifi_config));
    esp_wifi_connect();
  } 
  else if (event_base == USER_EVENTS && event_id == USER_WIFI_BTN) {
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    xEventGroupClearBits(s_wifi_event_group, S_CONNECTED_BIT);
    xTaskCreatePinnedToCore(smartconfig_task, "SMART_CONFIG_TASK", 4096, NULL, 3, NULL, 0);
  } 
  else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE) {
    ESP_LOGI(TAG, "Scan done");
  } 
  else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL) {
    ESP_LOGI(TAG, "Found channel");
  } 
  else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD) {
    ESP_LOGI(TAG, "Got SSID and password");

    smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
    wifi_config_t wifi_config;
    uint8_t ssid[MAX_SSID_LEN] = { 0 };
    uint8_t password[MAX_PASSPHRASE_LEN] = { 0 };
    uint8_t rvd_data[33] = { 0 };

    bzero(&wifi_config, sizeof(wifi_config_t));
    memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
    memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));

    memcpy(ssid, evt->ssid, sizeof(evt->ssid));
    memcpy(password, evt->password, sizeof(evt->password));
    ESP_LOGI(TAG, "SSID:%s", ssid);
    ESP_LOGI(TAG, "PASSWORD:%s", password);
    if (evt->type == SC_TYPE_ESPTOUCH_V2) {
        ESP_ERROR_CHECK( esp_smartconfig_get_rvd_data(rvd_data, sizeof(rvd_data)) );
        ESP_LOGI(TAG, "RVD_DATA:");
        for (int i=0; i<33; i++) 
        {
            printf("%02x ", rvd_data[i]);
        }
        printf("\n");
    }
    ESP_ERROR_CHECK( esp_wifi_disconnect() );
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    esp_wifi_connect();
    } 
    else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE) {
      xEventGroupSetBits(s_wifi_event_group, S_ESPTOUCH_DONE_BIT);
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
  if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
  if (length == 0) {
    nvs_set_str(s_wifi_nvs_handle, WIFI_SSID_KEY, "default");
    ESP_ERROR_CHECK(nvs_commit(s_wifi_nvs_handle));
  } else {
    ESP_ERROR_CHECK(nvs_get_str(s_wifi_nvs_handle, WIFI_SSID_KEY, (char*) &wifi_config.sta.ssid, &length));
  }
  err = nvs_get_str(s_wifi_nvs_handle, WIFI_PWD_KEY, NULL, &length);
  if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
  if (length == 0) {
    nvs_set_str(s_wifi_nvs_handle, WIFI_PWD_KEY, "1234567890");
    ESP_ERROR_CHECK(nvs_commit(s_wifi_nvs_handle));
  } else {
    ESP_ERROR_CHECK(nvs_get_str(s_wifi_nvs_handle, WIFI_PWD_KEY, (char*) &wifi_config.sta.password, &length));
  }

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START, &wifi_event_handle, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &wifi_event_handle, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handle, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(USER_EVENTS, USER_CHANGE_WIFI, &wifi_event_handle, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(USER_EVENTS, USER_WIFI_BTN, &wifi_event_handle, NULL));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_LOGI(TAG, "Waiting for IP");

  return ESP_OK;
}

esp_err_t set_wifi(char *p_ssid, char *p_pwd)
{
  xEventGroupSetBits(s_wifi_event_group, S_ESPTOUCH_DONE_BIT);
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
  if ((xEventGroupGetBits(s_wifi_event_group) & S_ESPTOUCH_START_BIT) == 0) {
    ESP_LOGI(TAG, "Not start");
    ESP_ERROR_CHECK(esp_event_post(USER_EVENTS, USER_WIFI_BTN, NULL, 0, portMAX_DELAY));
  }
  return ESP_OK;
}