#ifndef SENSORES_H
#define SENSORES_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

void sensores_init(void);
QueueHandle_t sensores_get_queue(void);

#endif // SENSORES_H

