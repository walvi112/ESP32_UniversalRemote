#include "ir_manage.h"
#include "esp_log.h"
#include "nvs.h"
#include "esp_timer.h"
#include "pin_config.h"

static const char *TAG = "IR_MANAGE";

static esp_timer_handle_t s_ir_timer_handle;
static esp_timer_create_args_t s_ir_timer_args;

static nvs_handle_t s_ir_handle;
static nvs_handle_t s_iri_handle;

SemaphoreHandle_t  ir_mutex;
static SemaphoreHandle_t ir_send_semp;

static TaskHandle_t s_ir_receive_task_handle;

IRMP_DATA irmp_data;
static long s_ir_code_id;
static long s_ir_remote_id;

static const char *s_ir_tv_key_name_array[IR_TV_NUM_REMOTE] = {IR_TV_1, IR_TV_2, IR_TV_3, IR_TV_4, IR_TV_5};
static IRMP_DATA s_ir_code_tv_array[IR_TV_NUM_REMOTE][IR_TV_NUM_CODE];
static char s_ir_code_tv_info_array[IR_TV_NUM_REMOTE][IR_INFO_LEN];

void ir_receive_task(void *args)
{
    while (1)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        TickType_t start_tick =  xTaskGetTickCount();
        TickType_t now_tick = start_tick;
        TickType_t previous_tick = 0;
        int led_state =  gpio_get_level(LED_PIN);
        uint8_t is_ir_detected = 0;
        while ((now_tick - start_tick) <= (5000 / portTICK_PERIOD_MS)) 
        {
            if (xSemaphoreTake(ir_mutex, 10 / portTICK_PERIOD_MS) == pdTRUE) {
                is_ir_detected = irmp_get_data(&irmp_data);
                xSemaphoreGive(ir_mutex);
            }     
            if (is_ir_detected == TRUE) {
                ir_add_code_tv(irmp_data, s_ir_code_id, s_ir_remote_id);
                ir_commit_tv(s_ir_remote_id);
                break;
            }
            if ((now_tick - previous_tick) * portTICK_PERIOD_MS >= 200) {
                previous_tick = now_tick;
                led_state ^= 1;
                gpio_set_level(LED_PIN, led_state);
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
            now_tick = xTaskGetTickCount();
        }
        gpio_set_level(LED_PIN, 1);   
        xSemaphoreGive(ir_send_semp);
    }
}

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
    ir_send_semp = xSemaphoreCreateBinary();
    if (ir_send_semp == NULL)
        return ESP_ERR_NO_MEM;
    xSemaphoreGive(ir_send_semp);    
    irmp_init();
    irsnd_init();
    s_ir_timer_args.callback = (void*) &ir_ISR;
    s_ir_timer_args.name = "ir_ISR";
    ESP_ERROR_CHECK(esp_timer_create(&s_ir_timer_args, &s_ir_timer_handle));
    ESP_ERROR_CHECK(esp_timer_start_periodic(s_ir_timer_handle, IR_PERIOD_US));
    xTaskCreatePinnedToCore(&ir_receive_task, "IR_RECEIVE_TASK", 2048, NULL, 2, &s_ir_receive_task_handle, 1);
    return ESP_OK;
}

esp_err_t ir_storage_init(void)
{
    esp_err_t err;
    size_t ir_code_array_size = 0;
    ESP_ERROR_CHECK(nvs_open(IR_NAMESPACE, NVS_READWRITE, &s_ir_handle));
    ESP_ERROR_CHECK(nvs_open(IRI_NAMESPACE, NVS_READWRITE, &s_iri_handle));
    
    for (int i = 0; i < IR_TV_NUM_REMOTE; i++) {
        ir_code_array_size = 0;
        err = nvs_get_blob(s_ir_handle, s_ir_tv_key_name_array[i], NULL, &ir_code_array_size);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
        if (ir_code_array_size == 0) {
            ESP_LOGI(TAG, "Empty IR code detected in TV remote %d", i + 1);
        } else {
            ESP_ERROR_CHECK(nvs_get_blob(s_ir_handle, s_ir_tv_key_name_array[i], s_ir_code_tv_array[i], &ir_code_array_size));
        }
        
        ir_code_array_size = 0;
        err = nvs_get_str(s_iri_handle, s_ir_tv_key_name_array[i], NULL, &ir_code_array_size);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
        if (ir_code_array_size == 0) {
            ESP_LOGI(TAG, "Empty IR information detected in TV remote %d", i + 1);
        } else {
            ESP_ERROR_CHECK(nvs_get_str(s_ir_handle, s_ir_tv_key_name_array[i], s_ir_code_tv_info_array[i], &ir_code_array_size));
        }
    }
    return ESP_OK;
}

esp_err_t ir_add_code_tv(IRMP_DATA ir_code, uint8_t ir_code_id, uint8_t ir_remote_id)
{
    if (ir_remote_id >= IR_TV_NUM_REMOTE) return ESP_FAIL;
    if (ir_code_id >= IR_TV_NUM_CODE) return ESP_FAIL;
    s_ir_code_tv_array[ir_remote_id][ir_code_id] = ir_code;
    return ESP_OK;
}

esp_err_t ir_add_code_info_tv(char *info, uint8_t ir_remote_id)
{
    if (ir_remote_id >= IR_TV_NUM_REMOTE) return ESP_FAIL;
    strncpy(s_ir_code_tv_info_array[ir_remote_id], info, IR_INFO_LEN - 1);
    return ESP_OK;
}

esp_err_t ir_commit_tv(uint8_t ir_remote_id)
{
    if (ir_remote_id >= IR_TV_NUM_REMOTE) return ESP_FAIL;
    if (nvs_set_blob(s_ir_handle, s_ir_tv_key_name_array[ir_remote_id], s_ir_code_tv_array[ir_remote_id], sizeof(IRMP_DATA) * IR_TV_NUM_CODE) != ESP_OK) 
        return ESP_FAIL;
    if (nvs_set_str(s_ir_handle, s_ir_tv_key_name_array[ir_remote_id], s_ir_code_tv_info_array[ir_remote_id]) != ESP_OK)
        return ESP_FAIL;
    ESP_ERROR_CHECK(nvs_commit(s_ir_handle));
    return ESP_OK;
}

esp_err_t ir_send_code_tv(long ir_code_id, long ir_remote_id)
{
    if ( ir_code_id < 0 || ir_code_id >= IR_TV_NUM_CODE) {
        ESP_LOGE(TAG, "Invalid ir code id");
        return ESP_FAIL;
    }
    if (ir_remote_id < 0 || ir_remote_id >= IR_TV_NUM_REMOTE) {
        ESP_LOGE(TAG, "Invalid ir remote id");
        return ESP_FAIL;
    }
    IRMP_DATA ir_to_send = s_ir_code_tv_array[ir_remote_id][ir_code_id];
    if (ir_to_send.address == 0 && ir_to_send.command == 0 && ir_to_send.flags == 0 && ir_to_send.protocol == 0) {
        ESP_LOGE(TAG, "IR code not existed");
        return ESP_FAIL;
    }
    if (xSemaphoreTake(ir_mutex, 10 / portTICK_PERIOD_MS) == pdTRUE) {
        ESP_LOGI(TAG, ">Sent IR: %d %d %d\n", ir_to_send.protocol, ir_to_send.address, ir_to_send.command);
        irsnd_send_data (&ir_to_send, TRUE);
        xSemaphoreGive(ir_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to obtain ir_mutex");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t ir_add_code_tv_detect(long ir_code_id, long ir_remote_id)
{    
    if (ir_code_id < 0 || ir_code_id >= IR_TV_NUM_CODE) {
        ESP_LOGE(TAG, "Invalid ir code id");
        return ESP_FAIL;
    } 
    if (ir_remote_id < 0 || ir_remote_id >= IR_TV_NUM_REMOTE) {
        ESP_LOGE(TAG, "Invalid ir remote id");
        return ESP_FAIL;
    }
    if (xSemaphoreTake(ir_send_semp, 10 / portTICK_PERIOD_MS) == pdTRUE) {
        s_ir_code_id = ir_code_id;
        s_ir_remote_id = ir_remote_id;
        xTaskNotifyGive(s_ir_receive_task_handle);
    }
    return ESP_OK;
}