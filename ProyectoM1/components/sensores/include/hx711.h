#pragma once
#include "driver/gpio.h"

void hx711_init(gpio_num_t dout, gpio_num_t sck);
int32_t hx711_read(void);