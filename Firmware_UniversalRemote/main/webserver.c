#include "webserver.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "ir_manage.h"
#include "wifi_connect.h"

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
    if (get_wifi_mode() == WIFI_MODE_AP) {
        httpd_resp_set_hdr(req, "Location", "/wifi");
    } else {
        httpd_resp_set_hdr(req, "Location", "/tv/1");
    }
    httpd_resp_send(req, NULL, 0); 
    return ESP_OK;
}

static esp_err_t http_resp_tv_remote(httpd_req_t *req) 
{   
    char *pch =strrchr(req->uri,'/');
    long num_dev = strtol(pch + 1, NULL, 10);
    
    if (num_dev > 0 && num_dev <= 5) {
        extern const unsigned char tv_remote_html_start[] asm("_binary_tv_remote_html_start");
        extern const unsigned char tv_remote_html_end[] asm("_binary_tv_remote_html_end");
        const size_t tv_remote_html_size = (tv_remote_html_end - tv_remote_html_start);

        httpd_resp_send_chunk(req, (const char *)tv_remote_html_start, tv_remote_html_size);
        httpd_resp_sendstr_chunk(req, NULL);
        return ESP_OK;
    } 

    httpd_resp_send_404(req);
    return ESP_FAIL;
}

static esp_err_t http_resp_tv_remote_command(httpd_req_t *req) 
{
    char *pch =strrchr(req->uri,'/');
    long num_dev = strtol(pch + 1, NULL, 10) - 1;
    
    long ir_code = 0;
    char buf[32];
    int ret, remaining = req->content_len;
    while (remaining > 0) 
    {
        if ((ret = httpd_req_recv(req, buf, (remaining > sizeof(buf) ? remaining : sizeof(buf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
        return ESP_FAIL;
        }
        remaining -= ret;
    }

    ir_code = strtol(buf, NULL, 10);
    if (strcmp(req->user_ctx, "command") == 0) {
        ESP_LOGI(TAG, "Sending IR code");
        ir_send_code_tv(ir_code, num_dev); 
    } else {
        ESP_LOGI(TAG, "Adding IR code");
        ir_add_code_tv_detect(ir_code, num_dev);
    }    
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t http_resp_ac_remote(httpd_req_t *req) 
{   
    char *pch =strrchr(req->uri,'/');
    long num_dev = strtol(pch + 1, NULL, 10);

    if (num_dev > 0 && num_dev <= 5) {
        extern const unsigned char ac_remote_html_start[] asm("_binary_ac_remote_html_start");
        extern const unsigned char ac_remote_html_end[] asm("_binary_ac_remote_html_end");
        const size_t ac_remote_html_size = (ac_remote_html_end - ac_remote_html_start);

        httpd_resp_send_chunk(req, (const char *)ac_remote_html_start, ac_remote_html_size);
        httpd_resp_sendstr_chunk(req, NULL);
        return ESP_OK;
    }

    httpd_resp_send_404(req);
    return ESP_FAIL;
}

static esp_err_t httpd_resp_setwifi(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        extern const unsigned char login_html_start [] asm("_binary_login_html_start");
        extern const unsigned char login_html_end [] asm("_binary_login_html_end");
        const size_t login_html_size = (login_html_end - login_html_start);

        httpd_resp_send_chunk(req, (const char*)login_html_start, login_html_size);
        httpd_resp_sendstr_chunk(req, NULL);
    } else {
        char buf[128];
        int ret, remaining = req->content_len;
        while (remaining > 0)
        {
            if ((ret = httpd_req_recv(req, buf, (remaining > sizeof(buf) ? remaining : sizeof(buf)))) <= 0) {
                if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                    continue;
                }
                return ESP_FAIL;
            }
            remaining -= ret;
        }
        char* form_ssid = strchr(buf, '=') + 1;
        char* form_pwd = strchr(form_ssid, '=') + 1;
        
        buf[req->content_len] = '\0';
        *strchr(buf, '&') = '\0';
        *strchr(buf, '+') = ' ';

        set_wifi(form_ssid, form_pwd);
        httpd_resp_send(req, NULL, 0);
    }
    return ESP_OK;
}

esp_err_t startwebserver(void) 
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 10;
    config.uri_match_fn = httpd_uri_match_wildcard;
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Error starting server");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Registering URI handlers");
    httpd_uri_t favicon = {
        .uri = "/favicon.ico",
        .method = HTTP_GET,
        .handler = http_resp_favicon,
        .user_ctx = NULL,
    
    };
    httpd_register_uri_handler(server, &favicon);
    
    httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = http_resp_root,
        .user_ctx = NULL,

    };
    httpd_register_uri_handler(server, &root);

    httpd_uri_t tv_remote = {
        .uri = "/tv/*",
        .method = HTTP_GET,
        .handler = http_resp_tv_remote,
        .user_ctx = NULL,

    };
    httpd_register_uri_handler(server, &tv_remote);

    httpd_uri_t add_tv_remote = {
        .uri = "/addtv/*",
        .method = HTTP_GET,
        .handler = http_resp_tv_remote,
        .user_ctx = NULL,

    };
    httpd_register_uri_handler(server, &add_tv_remote);

    httpd_uri_t ac_remote = {
        .uri = "/ac/*",
        .method = HTTP_GET,
        .handler = http_resp_ac_remote,
        .user_ctx = NULL,

    };
    httpd_register_uri_handler(server, &ac_remote);

    httpd_uri_t command_tv = {
        .uri = "/command/tv/*",
        .method = HTTP_POST,
        .handler = http_resp_tv_remote_command,
        .user_ctx = "command",
    };
    httpd_register_uri_handler(server, &command_tv);

    httpd_uri_t add_tv = {
        .uri = "/add/tv/*",
        .method = HTTP_POST,
        .handler = http_resp_tv_remote_command,
        .user_ctx = "add",
    };
    httpd_register_uri_handler(server, &add_tv);

    httpd_uri_t set_wifi_page = {
        .uri = "/wifi",
        .method = HTTP_GET,
        .handler = httpd_resp_setwifi,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &set_wifi_page);

    httpd_uri_t set_wifi_cmd = {
        .uri = "/wifi",
        .method = HTTP_POST,
        .handler = httpd_resp_setwifi,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &set_wifi_cmd);

    return ESP_OK;
}