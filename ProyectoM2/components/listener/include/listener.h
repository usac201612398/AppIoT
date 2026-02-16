#ifndef LISTENER_H
#define LISTENER_H

#include "mqtt_service.h" // para planta_state_t y tanque_state_t
extern bool manual_llenado;
extern bool manual_riego;
extern bool riego_activo;

void listener_init(void);
void listener_manual_llenado(const char *accion);
void listener_manual_riego(const char *accion);
//tanque_state_t mqtt_get_tanque_state(void);
void mqtt_parse_manual(const char *topic, const char *data);

#endif
