#include "sensores.h"
#include "dht11.h"
#include "sensor_msg.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#define DHT_PIN GPIO_NUM_4
#define QUEUE_LEN 10
#define CONNECTION_TIMEOUT 5

static const char *TAG = "SENSORES";

static QueueHandle_t sensor_queue;
static dht11_t dht11_sensor;

static void dht11_task(void *arg)
{
    sensor_msg_t msg;

    while (1) {
        if (dht11_read(&dht11_sensor, CONNECTION_TIMEOUT) == 0) {

            msg.sensor = SENSOR_DHT11;
            msg.valor1 = dht11_sensor.temperature;
            msg.valor2 = dht11_sensor.humidity;
            msg.timestamp_ms = esp_timer_get_time() / 1000;

            xQueueSend(sensor_queue, &msg, 0);

            ESP_LOGI(TAG, "Enviado a queue: T=%.1f H=%.1f",
                     msg.valor1, msg.valor2);
        }

        vTaskDelay(pdMS_TO_TICKS(2000)); // 2 segundos entre lecturas
    }
}

void sensores_init(void)
{
    sensor_queue = xQueueCreate(QUEUE_LEN, sizeof(sensor_msg_t));
    if (!sensor_queue) {
        ESP_LOGE(TAG, "Error creando la queue de sensores");
        return;
    }

    dht11_sensor.dht11_pin = DHT_PIN;

    xTaskCreatePinnedToCore(
        dht11_task,
        "dht11_task",
        4096,
        NULL,
        5,
        NULL,
        0
    );

    ESP_LOGI(TAG, "Queue y tarea DHT11 inicializadas");
}

QueueHandle_t sensores_get_queue(void)
{
    return sensor_queue;
}
