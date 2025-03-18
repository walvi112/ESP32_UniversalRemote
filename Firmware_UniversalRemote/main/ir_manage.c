#include "ir_manage.h"
#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "IR_MANAGE";

esp_err_t ir_storage_init(void)
{
    nvs_handle_t ir_handle;
    nvs_handle_t iri_handle;
    esp_err_t err;
    char *ir_tv_key_name[IR_TV_NUM_REMOTE] = {IR_TV_1, IR_TV_2, IR_TV_3, IR_TV_4, IR_TV_5};
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
        
        ir_code_array_size = 0;
        err = nvs_get_blob(iri_handle, ir_tv_key_name[i], NULL, &ir_code_array_size);
        ESP_LOGI(TAG, "Checking IR information storage");
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
        if (ir_code_array_size == 0) {
            ESP_LOGI(TAG, "Empty IR information detected in TV remote %d", i + 1);
        }
    }
    return ESP_OK;
}