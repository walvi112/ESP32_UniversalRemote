#include <stdio.h>
#include <stdint.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "irmp.h"
#include "irsnd.h"

#define IR_PERIOD_US        (1000000 / F_INTERRUPTS)
#define UART_BUFFER_SIZE     2048

#define LED_PIN              GPIO_NUM_17
#define KEY_PIN              GPIO_NUM_16

IRMP_DATA irmp_data;
esp_timer_handle_t ir_timer_handle;
TaskHandle_t led_toggle_task_handle;
TaskHandle_t ir_task_handle;
TaskHandle_t key_press_task_handle;

QueueHandle_t ir_mutex;
char uart_buffer[UART_BUFFER_SIZE];

void ir_ISR(void *args);
void led_toggle_task(void *args);
void ir_task(void *args);
void key_press_task(void *args);
void cli_task(void *args);

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

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << KEY_PIN),
        .pull_down_en = 0,
        .pull_up_en = 0
    };
    gpio_config(&io_conf);
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

    xTaskCreatePinnedToCore(&led_toggle_task, "LED_TOGGLE_TASK", 1024, NULL, 1, &led_toggle_task_handle, 1);
    xTaskCreatePinnedToCore(&ir_task, "IR_TASK", 1024, NULL, 1, &ir_task_handle, 1);
    xTaskCreatePinnedToCore(&key_press_task, "KEY_PRESS_TASK", 1024, NULL, 2, &key_press_task_handle, 1);
    xTaskCreatePinnedToCore(&cli_task, "CLI_TASK", 2048, NULL, 1, NULL, 1);
}

void ir_ISR(void *args)
{
    if (!irsnd_ISR()) {                                   
        irmp_ISR();                     
    }
}

void led_toggle_task(void *args)
{
    uint8_t level = 1;
    while (1)
    {
        gpio_set_level(LED_PIN, level);
        level ^= 1;
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

void ir_task(void *args) 
{
    while (1)
    {
        xSemaphoreTake(ir_mutex, portMAX_DELAY);
        if (irmp_get_data(&irmp_data)) {
            irmp_data.flags = 0;
            irsnd_send_data(&irmp_data, TRUE);
            printf("\nReceived %10s(%2d): addr=0x%04x cmd=0x%04x, f=%d ",
                irmp_protocol_names[ irmp_data.protocol],
                irmp_data.protocol,
                irmp_data.address,
                irmp_data.command,
                irmp_data.flags);
        }
        xSemaphoreGive(ir_mutex);
        vTaskDelay(10 / portTICK_PERIOD_MS);
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
    printf("Enter commands for testing: \n");
    printf("led on/ led off/ led toggle\n");
    printf("send ir (message_to_send)\n");
    int length = 0;
    while (1)
    {
        length = uart_read_bytes(UART_NUM_0, uart_buffer, UART_BUFFER_SIZE - 1, 20 / portTICK_PERIOD_MS);
        if (length) {
            uart_buffer[length] = '\0';
            uart_write_bytes(UART_NUM_0, uart_buffer, length);
            if (strncmp(uart_buffer, "led on", strlen("led on")) == 0) {
                vTaskSuspend(led_toggle_task_handle);
                gpio_set_level(LED_PIN, 1);
            }
            else if (strncmp(uart_buffer, "led off", strlen("led off")) == 0) {
                vTaskSuspend(led_toggle_task_handle);
                gpio_set_level(LED_PIN, 0);
            }
            else if (strncmp(uart_buffer, "led toggle", strlen("led toggle")) == 0) {
                vTaskResume(led_toggle_task_handle);
            }
            else if (strncmp(uart_buffer, "send ir ", strlen("send ir ")) == 0) {
                uint8_t token_count = 0;
                uint16_t ir_send[2];
                char *pch = strtok(uart_buffer + strlen("send ir "), " ");
                while (pch != NULL && token_count < 2) {
                    ir_send[token_count] = strtol(pch, NULL, 10);
                    pch = strtok(NULL, " ");
                    token_count += 1;    
                }
                
                if (token_count < 2) {
                    continue;
                }

                if (xSemaphoreTake(ir_mutex, 10 / portTICK_PERIOD_MS) == pdTRUE) {
                    irmp_data.address  = (uint8_t) ir_send[0];       
                    irmp_data.command  = ir_send[1];       
                    irmp_data.flags    = 0;
                    irmp_data.protocol = IRMP_NEC_PROTOCOL;
                    irsnd_send_data (&irmp_data, TRUE);
                    xSemaphoreGive(ir_mutex);
                }
            }
        }
    }    
}