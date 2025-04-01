#include "ir_manage.h"
#include "esp_log.h"
#include "nvs.h"
#include "esp_timer.h"

static const char *TAG = "IR_MANAGE";
QueueHandle_t ir_mutex;
esp_timer_handle_t ir_timer_handle;
static const char *ir_tv_key_name[IR_TV_NUM_REMOTE] = {IR_TV_1, IR_TV_2, IR_TV_3, IR_TV_4, IR_TV_5};
static nvs_handle_t ir_handle;
static nvs_handle_t iri_handle;
static IRMP_DATA ir_code_tv[IR_TV_NUM_REMOTE][IR_TV_NUM_CODE];
static char ir_code_tv_info[IR_TV_NUM_REMOTE][IR_INFO_LEN];

void ir_ISR(void *args)
{
    if (!irsnd_ISR()) {                                   
        irmp_ISR();                     
    }
}

esp_err_t ir_init(void)
{
    ir_mutex = xSemaphoreCreateMutex();
    if (ir_mutex == NULL)
        return ESP_ERR_NO_MEM;
    irmp_init();
    irsnd_init();
    esp_timer_create_args_t ir_timer_args;
    ir_timer_args.callback = (void*) &ir_ISR;
    ir_timer_args.name = "ir_ISR";
    ESP_ERROR_CHECK(esp_timer_create(&ir_timer_args, &ir_timer_handle));
    // ESP_ERROR_CHECK(esp_timer_start_periodic(ir_timer_handle, IR_PERIOD_US));
    return ESP_OK;
}

esp_err_t ir_storage_init(void)
{
    esp_err_t err;
    size_t ir_code_array_size = 0;
    ESP_ERROR_CHECK(nvs_open(IR_NAMESPACE, NVS_READWRITE, &ir_handle));
    ESP_ERROR_CHECK(nvs_open(IRI_NAMESPACE, NVS_READWRITE, &iri_handle));
    
    for (int i = 0; i < IR_TV_NUM_REMOTE; i++) {
        ir_code_array_size = 0;
        err = nvs_get_blob(ir_handle, ir_tv_key_name[i], NULL, &ir_code_array_size);
        ESP_LOGI(TAG, "Checking IR code storage");
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
        if (ir_code_array_size == 0) {
            ESP_LOGI(TAG, "Empty IR code detected in TV remote %d", i + 1);
        } 
        else {
            ESP_ERROR_CHECK(nvs_get_blob(ir_handle, ir_tv_key_name[i], ir_code_tv[i], &ir_code_array_size));
        }
        
        ir_code_array_size = 0;
        err = nvs_get_str(iri_handle, ir_tv_key_name[i], NULL, &ir_code_array_size);
        ESP_LOGI(TAG, "Checking IR information storage");
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
        if (ir_code_array_size == 0) {
            ESP_LOGI(TAG, "Empty IR information detected in TV remote %d", i + 1);
        } 
        else {
            ESP_ERROR_CHECK(nvs_get_str(ir_handle, ir_tv_key_name[i], ir_code_tv_info[i], &ir_code_array_size));
        }
    }
    return ESP_OK;
}

esp_err_t ir_add_code_tv(IRMP_DATA ir_code, uint8_t ir_code_id, uint8_t ir_remote_id)
{
    if (ir_remote_id >= IR_TV_NUM_REMOTE) return ESP_FAIL;
    if (ir_code_id >= IR_TV_NUM_CODE) return ESP_FAIL;
    ir_code_tv[ir_remote_id][ir_code_id] = ir_code;
    return ESP_OK;
}

esp_err_t ir_add_code_info_tv(char *info, uint8_t ir_remote_id)
{
    if (ir_remote_id >= IR_TV_NUM_REMOTE) return ESP_FAIL;
    strncpy(ir_code_tv_info[ir_remote_id], info, IR_INFO_LEN - 1);
    return ESP_OK;
}

esp_err_t ir_commit_tv(uint8_t ir_remote_id)
{
    if (ir_remote_id >= IR_TV_NUM_REMOTE) return ESP_FAIL;
    if (nvs_set_blob(ir_handle, ir_tv_key_name[ir_remote_id], ir_code_tv[ir_remote_id], sizeof(IRMP_DATA) * IR_TV_NUM_CODE) != ESP_OK) 
        return ESP_FAIL;
    if (nvs_set_str(ir_handle, ir_tv_key_name[ir_remote_id], ir_code_tv_info[ir_remote_id]) != ESP_OK)
        return ESP_FAIL;
    ESP_ERROR_CHECK(nvs_commit(ir_handle));
    return ESP_OK;
}

esp_err_t ir_send_code_tv(long code, long ir_remote_id)
{
    if (code >= IR_TV_NUM_CODE || code <= 0) {
        ESP_LOGE(TAG, "Invalid IR code");
        return ESP_FAIL;
    }
    if (ir_remote_id >= IR_TV_NUM_REMOTE || ir_remote_id <= 0) {
        ESP_LOGE(TAG, "Invalid remote id");
        return ESP_FAIL;
    }
    IRMP_DATA ir_to_send = ir_code_tv[ir_remote_id][code];
    if (ir_to_send.address == 0 && ir_to_send.command == 0 && ir_to_send.flags == 0 && ir_to_send.protocol == 0) {
        ESP_LOGE(TAG, "IR code not existed");
        return ESP_FAIL;
    }
    if (xSemaphoreTake(ir_mutex, 10 / portTICK_PERIOD_MS) == pdTRUE) {
        ESP_LOGI(TAG, ">Sent IR: %d %d %d\n", ir_to_send.protocol, ir_to_send.address, ir_to_send.command);
        irsnd_send_data (&ir_to_send, TRUE);
        xSemaphoreGive(ir_mutex);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to obtain ir_mutex");
    }
    return ESP_OK;
}

esp_err_t ir_add_code_tv_detect(long ir_code, long ir_remote_id)
{
    return ESP_OK;
}