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
    float nivel;
    float caudal;
    float temp_agua;

} tanque_state_t;

static tanque_state_t tanque_state;
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
             "}",
             tanque_state.nivel,
             tanque_state.caudal,
             tanque_state.temp_agua);

    esp_mqtt_client_publish(
        mqtt_get_client(),
        "casa/tanque01/data",
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

    while (1) {
        if (xQueueReceive(q, &msg, portMAX_DELAY)) {

            switch (msg.sensor) {

                case SENSOR_CAUDAL:
                    tanque_state.caudal = msg.valor1;
                    break;

                case SENSOR_ULTRASONICO:
                    tanque_state.nivel = msg.valor1;
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
    xTaskCreatePinnedToCore(
        listener_task,
        "listener_task",
        4096,
        NULL,
        6,
        NULL,
        1
    );

    ESP_LOGI(TAG, "Listener iniciado");
}
