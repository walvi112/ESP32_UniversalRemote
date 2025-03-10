#include "webserver.h"
#include "esp_http_server.h"
#include "esp_log.h"

static const char *TAG = "WEBSERVER";

static esp_err_t http_resp_favicon(httpd_req_t *req)
{
    extern const unsigned char favicon_ico_start[] asm("_binary_favicon_ico_start");
    extern const unsigned char favicon_ico_end[]   asm("_binary_favicon_ico_end");
    const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_size);
    return ESP_OK;
}

static esp_err_t http_resp_root(httpd_req_t *req) 
{   
    httpd_resp_set_status(req, "307 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "/tv");
    httpd_resp_send(req, NULL, 0); 
    return ESP_OK;
}

static esp_err_t http_resp_tv_remote(httpd_req_t *req) 
{   
    extern const unsigned char tv_remote_html_start[] asm("_binary_tv_remote_html_start");
    extern const unsigned char tv_remote_html_end[] asm("_binary_tv_remote_html_end");
    const size_t tv_remote_html_size = (tv_remote_html_end - tv_remote_html_start);

    httpd_resp_send_chunk(req, (const char *)tv_remote_html_start, tv_remote_html_size);
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

static esp_err_t http_resp_ac_remote(httpd_req_t *req) 
{   
    extern const unsigned char ac_remote_html_start[] asm("_binary_ac_remote_html_start");
    extern const unsigned char ac_remote_html_end[] asm("_binary_ac_remote_html_end");
    const size_t ac_remote_html_size = (ac_remote_html_end - ac_remote_html_start);

    httpd_resp_send_chunk(req, (const char *)ac_remote_html_start, ac_remote_html_size);
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

esp_err_t startwebserver(void) 
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);

    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Error starting server");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Registering URI handlers");
    httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = http_resp_root,
        .user_ctx = NULL,

    };
    httpd_register_uri_handler(server, &root);

    httpd_uri_t tv_remote = {
        .uri = "/tv",
        .method = HTTP_GET,
        .handler = http_resp_tv_remote,
        .user_ctx = NULL,

    };
    httpd_register_uri_handler(server, &tv_remote);

    httpd_uri_t ac_remote = {
        .uri = "/ac",
        .method = HTTP_GET,
        .handler = http_resp_ac_remote,
        .user_ctx = NULL,

    };
    httpd_register_uri_handler(server, &ac_remote);

    httpd_uri_t favicon = {
        .uri = "/favicon.ico",
        .method = HTTP_GET,
        .handler = http_resp_favicon,
        .user_ctx = NULL,

    };
    httpd_register_uri_handler(server, &favicon);

    return ESP_OK;
}