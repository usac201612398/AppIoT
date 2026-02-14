#include "listener.h"
#include "mqtt_service.h"
#include "sensores.h"
#include "sensor_msg.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"

#define PIN_VALVULA_LLENADO 27
#define RELAY_ACTIVE_LEVEL 0
#define NIVEL_MIN 20.0 // cm o porcentaje (según tu medición)
#define NIVEL_MAX 80.0
static bool valvula_llenado_activa = false;

static const char *TAG = "LISTENER";

typedef struct
{
    float nivel;
    float porcentaje_llenado;
    float caudal;
    float temp_agua;
} tanque_state_t;

static tanque_state_t tanque_state = {
    .nivel = 0,
    .caudal = 0,
    .temp_agua = 0,
    .porcentaje_llenado = 0};

static void publish_tanque_state(void)
{
    if (!mqtt_is_connected())
        return;

    char json[256];

    snprintf(json, sizeof(json),
             "{"
             "\"nivel\":%.2f,"
             "\"caudal\":%.2f,"
             "\"temp_agua\":%.2f"
             "\"porcen-llenado\":%.2f"
             "}",
             tanque_state.nivel,
             tanque_state.caudal,
             tanque_state.temp_agua,
             tanque_state.porcentaje_llenado);

    esp_mqtt_client_publish(
        mqtt_get_client(),
        "casa/tanque01/data",
        json,
        0,
        1,
        0);
}
static void valvula_on(void)
{
    gpio_set_level(PIN_VALVULA_LLENADO, RELAY_ACTIVE_LEVEL);
}

static void valvula_off(void)
{
    gpio_set_level(PIN_VALVULA_LLENADO, !RELAY_ACTIVE_LEVEL);
}
static void listener_task(void *arg)
{
    QueueHandle_t q = sensores_get_queue();
    sensor_msg_t msg;
    TickType_t last_publish = xTaskGetTickCount();

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
                // ---- CONTROL DE LLENADO ----
                if (!valvula_llenado_activa && tanque_state.porcentaje_llenado < NIVEL_MIN)
                {
                    valvula_on();
                    valvula_llenado_activa = true;
                    ESP_LOGI(TAG, "Valvula ACTIVADA (nivel bajo)");
                }

                else if (valvula_llenado_activa && tanque_state.porcentaje_llenado > NIVEL_MAX)
                {
                    valvula_off();
                    valvula_llenado_activa = false;
                    ESP_LOGI(TAG, "Valvula DESACTIVADA (nivel alto)");
                }
                break;

            case SENSOR_TEMP:
                tanque_state.temp_agua = msg.valor1;
                break;

            default:

                break;
            }
        }
        if (xTaskGetTickCount() - last_publish > pdMS_TO_TICKS(5000))
        {
            publish_tanque_state();
            last_publish = xTaskGetTickCount();
        }
    }
}

void listener_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_VALVULA_LLENADO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&io_conf);

    valvula_off();

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
