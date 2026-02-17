#include "listener.h"
#include "mqtt_service.h"
#include "sensores.h"
#include "sensor_msg.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "cJSON.h"

#define PIN_VALVULA_LLENADO 27
#define PIN_VALVULA_RIEGO 26
#define RELAY_ACTIVE_LEVEL 0

#define NIVEL_MIN 20.0        // porcentaje
#define NIVEL_MAX 80.0        // porcentaje
#define TIEMPO_RIEGO_MS 20000 // Son 20 segundos de riego

static const char *TAG = "LISTENER";
static tanque_state_t tanque_state = {0};
static bool valvula_llenado_activa = false;
bool riego_activo = false;
bool riego_manual = false;
bool tanque_manual = false;

static void valvula_on(int valvula)
{
    gpio_set_level(valvula, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));
}

static void valvula_off(int valvula)
{
    gpio_set_level(valvula, 1);
    vTaskDelay(pdMS_TO_TICKS(1000));
}

static void publish_tanque_state(void)
{
    if (!mqtt_is_connected())
        return;

    cJSON *root = cJSON_CreateObject();
    if (!root)
        return;
    cJSON_AddStringToObject(root,"tanque_id","tanque_0001");
    cJSON_AddNumberToObject(root, "nivel", tanque_state.nivel);
    cJSON_AddNumberToObject(root, "caudal", tanque_state.caudal);
    cJSON_AddNumberToObject(root, "temp_agua", tanque_state.temp_agua);
    cJSON_AddNumberToObject(root, "porcentaje_llenado", tanque_state.porcentaje_llenado);

    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str)
    {
        esp_mqtt_client_publish(
            mqtt_get_client(),
            "casa/tanque01/data",
            json_str,
            0,
            1,
            0);
        free(json_str);
    }

    cJSON_Delete(root);
}

void listener_manual_llenado(const char *accion)
{
    if (strcmp(accion, "ON") == 0)
    {
        // bloquea automático y fuerza encendido
        tanque_manual = true;
        valvula_on(PIN_VALVULA_LLENADO);
        mqtt_publicar_estado_valvula(true, tanque_state.porcentaje_llenado);
    }
    else if (strcmp(accion, "OFF") == 0)
    {
        // bloquea automático y fuerza apagado
        tanque_manual = true;
        valvula_off(PIN_VALVULA_LLENADO);
        mqtt_publicar_estado_valvula(false, tanque_state.porcentaje_llenado);
    }
    else if (strcmp(accion, "AUTO") == 0)
    {
        tanque_manual = false;
        // toma decisión inmediata
        if (tanque_state.porcentaje_llenado < NIVEL_MIN)
            valvula_on(PIN_VALVULA_LLENADO);
        else if (tanque_state.porcentaje_llenado > NIVEL_MAX)
            valvula_off(PIN_VALVULA_LLENADO);
    }
}

void listener_manual_riego(const char *accion)
{
    if (strcmp(accion, "ON") == 0)
    {
        // Modo manual ON → fuerza riego activo
        riego_manual = true; // bloquea automático
        riego_activo = true; // marca que está activo

        valvula_on(PIN_VALVULA_RIEGO);
        mqtt_publicar_estado_riego(true);
    }
    else if (strcmp(accion, "OFF") == 0)
    {
        // Modo manual OFF → fuerza riego inactivo
        riego_manual = true;  // bloquea automático
        riego_activo = false; // estado manual OFF

        valvula_off(PIN_VALVULA_RIEGO);
        mqtt_publicar_estado_riego(false);
    }
    else if (strcmp(accion, "AUTO") == 0)
    {
        // Salir de modo manual → vuelve automático
        riego_manual = false;
    }
}

static void listener_task(void *arg)
{
    QueueHandle_t q = sensores_get_queue();
    sensor_msg_t msg;
    TickType_t last_publish = xTaskGetTickCount();
    TickType_t riego_end = 0;

    while (1)
    {
        if (xQueueReceive(q, &msg, portMAX_DELAY))
        {
            switch (msg.sensor)
            {
            case SENSOR_CAUDAL:
                tanque_state.caudal = msg.valor1;
                break;

            case SENSOR_ULTRASONICO:
                tanque_state.nivel = msg.valor1;
                tanque_state.porcentaje_llenado = msg.valor2;

                if (!tanque_manual)
                {
                    if (!valvula_llenado_activa && tanque_state.porcentaje_llenado < NIVEL_MIN)
                    {
                        valvula_on(PIN_VALVULA_LLENADO);
                        valvula_llenado_activa = true;
                        mqtt_publicar_estado_valvula(true, tanque_state.porcentaje_llenado);
                    }
                    else if (valvula_llenado_activa && tanque_state.porcentaje_llenado > NIVEL_MAX)
                    {
                        valvula_off(PIN_VALVULA_LLENADO);
                        valvula_llenado_activa = false;
                        mqtt_publicar_estado_valvula(false, tanque_state.porcentaje_llenado);
                    }
                }

                break;

            case SENSOR_TEMP:
                tanque_state.temp_agua = msg.valor1;
                break;

            default:

                break;
            }
        }

        // Obtener estado de planta inbound
        planta_state_t planta = mqtt_get_planta_state();
        TickType_t ahora = xTaskGetTickCount();

        // Si no estamos en modo manual, aplicar automático
        if (!riego_manual)
        {
            if (!riego_activo &&
                planta.temp_amb > 34.0 &&
                planta.hum_amb < 34.0 &&
                planta.peso < 0.382 &&
                planta.hum_suelo < 15.0)
            {
                ESP_LOGI(TAG, "Activando riego automático");

                valvula_on(PIN_VALVULA_RIEGO);
                riego_activo = true; // marca que hay riego automático
                riego_end = ahora + pdMS_TO_TICKS(TIEMPO_RIEGO_MS);

                mqtt_publicar_estado_riego(true);
            }
        }

        // Si está activo (manual o automático) y se terminó el tiempo
        if (riego_activo && !riego_manual && ahora >= riego_end)
        {
            ESP_LOGI(TAG, "Riego automático finalizado");

            valvula_off(PIN_VALVULA_RIEGO);
            riego_activo = false;

            mqtt_publicar_estado_riego(false);
        }

        // --- Publicar estado del tanque cada 5s ---
        if (ahora - last_publish > pdMS_TO_TICKS(2000))
        {
            publish_tanque_state();
            last_publish = ahora;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void listener_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_VALVULA_LLENADO) | (1ULL << PIN_VALVULA_RIEGO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&io_conf);

    valvula_off(PIN_VALVULA_LLENADO);
    valvula_off(PIN_VALVULA_RIEGO);

    xTaskCreatePinnedToCore(
        listener_task,
        "listener_task",
        4096,
        NULL,
        6,
        NULL,
        1);

    ESP_LOGI(TAG, "Listener iniciado");
}