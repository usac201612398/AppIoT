#ifndef SENSORES_TANQUE_H
#define SENSORES_TANQUE_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

void sensores_init(void);

QueueHandle_t sensores_get_queue(void);

#endif


