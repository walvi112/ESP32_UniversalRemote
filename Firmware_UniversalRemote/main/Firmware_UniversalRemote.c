#include <stdio.h>
#include <stdint.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "irmp.h"
#include "irsnd.h"
#include "wifi_connect.h"
#include "webserver.h"
#define IR_READ_PERIOD_US   (1000000 / F_INTERRUPTS)

#define LED_PIN              GPIO_NUM_17

IRMP_DATA irmp_data;

esp_timer_handle_t ir_read_task_handle;

void app_main(void)
{   
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    irmp_init();
    esp_timer_create_args_t ir_read_timer_args;
    ir_read_timer_args.callback = (void*) &irmp_ISR;
    ir_read_timer_args.name = "irmp_ISR";
    esp_timer_create(&ir_read_timer_args, &ir_read_task_handle);
    esp_timer_start_periodic(ir_read_task_handle, IR_READ_PERIOD_US);

    gpio_dump_io_configuration(stdout, (1ULL << LED_PIN) | (1ULL << 13));
    connect_wifi();
    startwebserver();
}
