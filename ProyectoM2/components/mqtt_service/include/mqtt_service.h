#ifndef MQTT_SERVICE_H
#define MQTT_SERVICE_H

#include <stdbool.h>
#include "mqtt_client.h"

// Tipos compartidos
typedef struct {
    float temp_amb;
    float hum_amb;
    float hum_suelo;
    float peso;
} planta_state_t;

typedef struct {
    float nivel;
    float porcentaje_llenado;
    float caudal;
    float temp_agua;
} tanque_state_t;

//funciones publicas
planta_state_t mqtt_get_planta_state(void);

void mqtt_service_init(void);
bool mqtt_is_connected(void);
esp_mqtt_client_handle_t mqtt_get_client(void);
void mqtt_publicar_estado_valvula(bool abierta, float porcentaje);
void mqtt_publicar_estado_riego(bool activo);
void listener_manual_llenado(const char *accion);
void listener_manual_riego(const char *accion);

#endif // MQTT_SERVICE_H
