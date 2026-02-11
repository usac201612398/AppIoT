#include "listener.h"

#include "sensores.h"
#include "sensor_msg.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"

static const char *TAG = "LISTENER";

static void listener_task(void *arg)
{
    QueueHandle_t q = sensores_get_queue();
    sensor_msg_t msg;

    while (1) {
        if (xQueueReceive(q, &msg, portMAX_DELAY)) {

            switch (msg.sensor) {

                case SENSOR_DHT11:
                    ESP_LOGI(TAG,
                        "[DHT11] Temp=%.1f C  Hum=%.1f %%  (%lu ms)",
                        msg.valor1,
                        msg.valor2,
                        msg.timestamp_ms
                    );
                    break;

                case SENSOR_PESO:
                    ESP_LOGI(TAG,
                        "[PESO] %.2f kg",
                        msg.valor1
                    );
                    break;

                case SENSOR_SUELO:
                    ESP_LOGI(TAG,
                        "[SUELO] Humedad=%.1f %%",
                        msg.valor1
                    );
                    break;

                default:
                    ESP_LOGW(TAG, "Sensor desconocido");
                    break;
            }
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
