#ifndef DHT_11
#define DHT_11

#include <driver/gpio.h>
#include <stdio.h>
#include <string.h>
#include <rom/ets_sys.h>
#include "esp_log.h"

typedef struct
{
    int dht11_pin;
    float temperature;
    float humidity;
} dht11_t;

// Cambiado a puntero para consistencia
int wait_for_state(dht11_t *dht11, int state, int timeout);
void hold_low(dht11_t *dht11, int hold_time_us);
int dht11_read(dht11_t *dht11, int connection_timeout);

#endif // DHT_11
