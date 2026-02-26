// wifi.h
#ifndef WIFI_H
#define WIFI_H
#include <stdbool.h> 
#include "mqtt_service.h"

void wifi_init_sta(void);
bool wifi_is_connected(void);

#endif