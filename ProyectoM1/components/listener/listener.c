#include "listener.h"
#include "mqtt_service.h"

#include "sensores.h"
#include "sensor_msg.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"

static const char *TAG = "LISTENER";

typedef struct
{
    float soil;
    float weight;
    float temp_air;
    float hum_air;
} planta_state_t;

static planta_state_t planta_state;
static void publish_planta_state(void)
{
    if (!mqtt_is_connected())
        return;

    char json[256];

    snprintf(json, sizeof(json),
             "{"
             "\"soil\":%.2f,"
             "\"weight\":%.2f,"
             "\"temp_air\":%.2f,"
             "\"hum_air\":%.2f"
             "}",
             planta_state.soil,
             planta_state.weight,
             planta_state.temp_air,
             planta_state.hum_air);

    esp_mqtt_client_publish(
        mqtt_get_client(),
        "casa/planta01/data",
        json,
        0,
        1,
        0);
}

static void listener_task(void *arg)
{
    QueueHandle_t q = sensores_get_queue();
    sensor_msg_t msg;
    TickType_t last_publish = xTaskGetTickCount();

    while (1)
    {
        if (xQueueReceive(q, &msg, pdMS_TO_TICKS(100)))
        {
            switch (msg.sensor)
            {
            case SENSOR_SUELO:
                planta_state.soil = msg.valor1;
                break;

            case SENSOR_PESO:
                planta_state.weight = msg.valor1;
                break;

            case SENSOR_DHT11:
                planta_state.temp_air = msg.valor1;
                planta_state.hum_air = msg.valor2;
                break;

            default:
                break;
            }
        }

        if (xTaskGetTickCount() - last_publish > pdMS_TO_TICKS(5000))
        {
            publish_planta_state();
            last_publish = xTaskGetTickCount();
        }
    }
}

void listener_init(void)
{
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
