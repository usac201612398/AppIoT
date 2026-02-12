#ifndef SENSOR_MSG_H
#define SENSOR_MSG_H

#include <stdint.h>

typedef enum {
    SENSOR_ULTRASONICO,
    SENSOR_TEMP,
    SENSOR_CAUDAL
} sensor_id_t;

typedef struct {
    sensor_id_t sensor;
    float valor1;
    float valor2;
    uint32_t timestamp_ms;
} sensor_msg_t;

#endif // SENSOR_MSG_H
