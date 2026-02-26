#include "listener.h"
#include "mqtt_service.h"
#include "sensores.h"
#include "sensor_msg.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_random.h"

static const char *TAG = "LISTENER";

typedef struct
{
    float hum_suelo;
    float peso;
    float temp_amb;
    float hum_amb;
    float ts;
} planta_state_t;

static planta_state_t planta_state;

static void publish_planta_state(void)
{
    if (!mqtt_is_connected())
        return;

    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
        return; // Manejar error

    cJSON_AddStringToObject(root,"planta_id","planta_0001");
    cJSON_AddNumberToObject(root, "hum_suelo", planta_state.hum_suelo);
    cJSON_AddNumberToObject(root, "peso", planta_state.peso);
    cJSON_AddNumberToObject(root, "temp_amb", planta_state.temp_amb);
    cJSON_AddNumberToObject(root, "hum_amb", planta_state.hum_amb);
    cJSON_AddNumberToObject(root, "ts", planta_state.ts);

    char *json_str = cJSON_PrintUnformatted(root); // Sin espacios innecesarios
    if (json_str != NULL)
    {
        //vTaskDelay(pdMS_TO_TICKS(50 + (esp_random() % 50))); // 50-100 ms de delay
        esp_mqtt_client_publish(
            mqtt_get_client(),
            "casa/planta/1/data",
            json_str,
            0,
            1,
            0);

        free(json_str); // Liberar memoria
    }
    cJSON_Delete(root);
}

static void listener_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(esp_random() % 100));
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
                planta_state.hum_suelo = msg.valor1;
                break;

            case SENSOR_PESO:
                planta_state.peso = msg.valor1;
                break;

            case SENSOR_DHT11:
                planta_state.temp_amb = msg.valor1;
                planta_state.hum_amb = msg.valor2;
                break;

            default:
                break;
            }
        }

        if (xTaskGetTickCount() - last_publish > pdMS_TO_TICKS(30000))
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
