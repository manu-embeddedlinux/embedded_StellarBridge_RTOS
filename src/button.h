#ifndef BUTTON_H
#define BUTTON_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

// Function prototypes
// void button_init(gpio_num_t button_pin, gpio_num_t latch_pin, gpio_num_t disp_ctrl_pin);

void button_monitoring_task(void* pvParameters);

void configure_gpio();

#endif // BUTTON_H
