#ifndef LISTENER_H
#define LISTENER_H
#include "mqtt_service.h" 

extern bool manual_llenado;
extern bool manual_riego;
extern bool riego_activo;

void listener_init(void);
void listener_manual_llenado(const char *accion);
//void listener_manual_riego(const char *accion);
//tanque_state_t mqtt_get_tanque_state(void);
void listener_riego_manual(int zona, const char *accion, int tiempo_ms);

#endif
