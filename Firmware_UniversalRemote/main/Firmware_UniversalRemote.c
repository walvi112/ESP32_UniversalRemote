#include <stdio.h>
#include <stdint.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "irmp.h"
#include "irsnd.h"
#include "wifi_connect.h"
#include "webserver.h"

#define IR_PERIOD_US        (1000000 / F_INTERRUPTS)
#define UART_BUFFER_SIZE     2048

#define LED_PIN              GPIO_NUM_17
#define KEY_PIN              GPIO_NUM_16

IRMP_DATA irmp_data;
esp_timer_handle_t ir_timer_handle;
TaskHandle_t key_press_task_handle;

QueueHandle_t ir_mutex;
char uart_buffer[UART_BUFFER_SIZE];

void ir_ISR(void *args);
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

    irmp_init();
    irsnd_init();
    esp_timer_create_args_t ir_timer_args;
    ir_timer_args.callback = (void*) &ir_ISR;
    ir_timer_args.name = "ir_ISR";
    esp_timer_create(&ir_timer_args, &ir_timer_handle);
    esp_timer_start_periodic(ir_timer_handle, IR_PERIOD_US);

    gpio_dump_io_configuration(stdout, (1ULL << LED_PIN) | (1ULL << KEY_PIN));
    ir_mutex = xSemaphoreCreateMutex();

    xTaskCreatePinnedToCore(&cli_task, "CLI_TASK", 4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(&key_press_task, "KEY_PRESS_TASK", 1024, NULL, 2, &key_press_task_handle, 1);

    connect_wifi();
    startwebserver();
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
    char *pch = strtok(input, " ");
    while (pch != NULL && token_count < array_length) {
        printf("%s\n", pch);
        output_array[token_count] = pch;
        pch = strtok(NULL, " ");
        token_count += 1;
    }
    if (token_count < array_length) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

void ir_ISR(void *args)
{
    if (!irsnd_ISR()) {                                   
        irmp_ISR();                     
    }
}

void key_press_task(void *args)
{
    while (1)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        printf("User key pressed\n");
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
             if (strncmp(uart_buffer, "led on", strlen("led on")) == 0) {
                printf(">Led on\n");
                gpio_set_level(LED_PIN, 1);
            }
            else if (strncmp(uart_buffer, "led off", strlen("led off")) == 0) {
                printf(">Led off\n");
                gpio_set_level(LED_PIN, 0);
            }
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
            else if (strncmp(uart_buffer, "set wifi ", strlen("set wifi ")) == 0) {
                char *wifi_new[2];
                if(str_to_parram_str(uart_buffer + strlen("set wifi "), wifi_new, 2) == ESP_FAIL) {
                    continue;
                }
                printf(">Set WIFI SSID: %s\n", wifi_new[0]);
                set_wifi(wifi_new[0], wifi_new[1]);
            }            
        }
    }    
}