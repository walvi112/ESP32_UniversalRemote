#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "wifi_connect.h"
#include "webserver.h"
#include "ir_manage.h"
#include "pin_config.h"

#define UART_BUFFER_SIZE     2048

TaskHandle_t key_press_task_handle;
char uart_buffer[UART_BUFFER_SIZE];

void cli_task(void *args);
void key_press_task(void *args);

static void IRAM_ATTR key_isr_handler(void *args)
{
    BaseType_t xHigherPriorityTaskWoken;
    vTaskNotifyGiveFromISR(key_press_task_handle, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

void app_main(void)
{   
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    const uart_port_t uart_num = UART_NUM_0;
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(uart_num, GPIO_NUM_1, GPIO_NUM_3, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(uart_num, UART_BUFFER_SIZE, 0, 0, NULL, 0));

    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN, 1);

    gpio_reset_pin(KEY_PIN);
    gpio_set_direction(KEY_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(KEY_PIN, GPIO_FLOATING);
    gpio_set_intr_type(KEY_PIN, GPIO_INTR_NEGEDGE);

    gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    gpio_isr_handler_add(KEY_PIN, key_isr_handler, (void*) KEY_PIN);
    gpio_dump_io_configuration(stdout, (1ULL << LED_PIN) | (1ULL << KEY_PIN));

    ESP_ERROR_CHECK(ir_init());
    ESP_ERROR_CHECK(ir_storage_init());
    ESP_ERROR_CHECK(connect_wifi());
    ESP_ERROR_CHECK(startwebserver());

    xTaskCreatePinnedToCore(&cli_task, "CLI_TASK", 4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(&key_press_task, "KEY_PRESS_TASK", 1024, NULL, 2, &key_press_task_handle, 1);
}

esp_err_t str_to_parram_int(char *input, int *output_array, unsigned int array_length)
{
    uint8_t token_count = 0;
    char *pch = strtok(input, " ");
    while (pch != NULL && token_count < array_length) {
        output_array[token_count] = strtol(pch, NULL, 10);
        pch = strtok(NULL, " ");
        token_count += 1;    
    }
    if (token_count < array_length) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t str_to_parram_str(char *input, char **output_array, unsigned int array_length)
{
    uint8_t token_count = 0;
    char *pch = strtok(input, "+");
    while (pch != NULL && token_count < array_length) {
        printf("%s\n", pch);
        output_array[token_count] = pch;
        pch = strtok(NULL, "+");
        token_count += 1;
    }
    if (token_count < array_length) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

void key_press_task(void *args)
{
    TickType_t now_tick = 0;
    TickType_t previous_tick = 0;
    while (1)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        now_tick = xTaskGetTickCount();
        if ((now_tick - previous_tick) >= (DEBOUNCE_PERIOD_MS / portTICK_PERIOD_MS)) {
            previous_tick = now_tick;
            printf("User key pressed\n");
            reset_wifi();
        }
    }
}

void cli_task(void *args)
{
    int length = 0;
    while (1)
    {
        length = uart_read_bytes(UART_NUM_0, uart_buffer, UART_BUFFER_SIZE - 1, 20 / portTICK_PERIOD_MS);
        if (length) {
            uart_buffer[length] = '\0';
            uart_write_bytes(UART_NUM_0, uart_buffer, length);
            // led on : turn on indicator led
            if (strncmp(uart_buffer, "led on", strlen("led on")) == 0) {
                printf(">Led on\n");
                gpio_set_level(LED_PIN, 1);
            }
            // led off : turn off indicator led
            else if (strncmp(uart_buffer, "led off", strlen("led off")) == 0) {
                printf(">Led off\n");
                gpio_set_level(LED_PIN, 0);
            }
            // send ir protocol+address+command : send ir code
            else if (strncmp(uart_buffer, "send ir ", strlen("send ir ")) == 0) {
                int ir_send[3];
                if (str_to_parram_int(uart_buffer + strlen("send ir "), ir_send, 3) == ESP_FAIL) {
                    continue;
                }
                if (xSemaphoreTake(ir_mutex, 10 / portTICK_PERIOD_MS) == pdTRUE) {
                    printf(">Sent IR: %d %d %d\n", ir_send[0], ir_send[1], ir_send[2]);
                    irmp_data.address  = (uint16_t) ir_send[1];       
                    irmp_data.command  = (uint16_t) ir_send[2];       
                    irmp_data.flags    = 0;
                    irmp_data.protocol = (uint8_t) ir_send[0];
                    irsnd_send_data (&irmp_data, TRUE);
                    xSemaphoreGive(ir_mutex);
                }
            }
            // set wifi ssid+pwd : set wifi 
            else if (strncmp(uart_buffer, "set wifi ", strlen("set wifi ")) == 0) {
                char *wifi_new[2];
                if(str_to_parram_str(uart_buffer + strlen("set wifi "), wifi_new, 2) == ESP_FAIL) {
                    printf(">Format should be: set wifi ssid+pwd.\n");
                    continue;
                }
                char *ptr;
                ptr = strrchr(wifi_new[1], '\r');
                if (ptr != NULL) {
                    *ptr = '\0';
                }
                ptr = strrchr(wifi_new[1], '\n');
                if (ptr != NULL) {
                    *ptr = '\0';
                }
                if (strlen(wifi_new[0]) >= MAX_WIFI_SSID_LENGTH || strlen(wifi_new[1]) >= MAX_WIFI_PWD_LENGTH) {
                    printf(">SSID or PWD to long.\n");    
                }
                else {
                    printf(">Set WIFI SSID: %s\n", wifi_new[0]);
                    set_wifi(wifi_new[0], wifi_new[1]);
                }
            }
            // add tv ir ir_code remote_id : enter add tv ir mode
            else if (strncmp(uart_buffer, "add tv ir ", strlen("add tv ir ")) == 0) {
                int ir_receive[2];
                if (str_to_parram_int(uart_buffer + strlen("add tv ir "), ir_receive, 2) == ESP_FAIL) {
                    continue;
                }
                printf(">Add TV IR, please point the TV remote to the receiver and press key.\n");
                ir_add_code_tv_detect(ir_receive[0], ir_receive[1]);
            } 
            // restart : restart device
            else if (strncmp(uart_buffer, "restart", strlen("restart")) == 0) {
                printf(">Restart device.\n");
                esp_restart();
            }
            else if (strncmp(uart_buffer, "reset wifi", strlen("reset wifi")) == 0) {
                printf(">Reset wifi.\n");
                reset_wifi();
            }
        }
    }    
}