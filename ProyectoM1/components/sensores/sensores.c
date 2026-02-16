#include "sensores.h"
#include "dht11.h"
#include "sensor_msg.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_adc/adc_oneshot.h"
#include "hx711.h"

#define DHT_PIN GPIO_NUM_4              // GPIO4
#define SUELO_ADC_CHANNEL ADC_CHANNEL_6 // GPIO34

#define HX711_DOUT GPIO_NUM_18
#define HX711_SCK GPIO_NUM_19

#define SUELO_MOJADO 1350
#define SUELO_SECO 3130
#define QUEUE_LEN 10
#define CONNECTION_TIMEOUT 5

static const char *TAG = "SENSORES";
static adc_oneshot_unit_handle_t adc_handle;
static QueueHandle_t sensor_queue;
static dht11_t dht11_sensor;
static int32_t peso_offset = -124000;
static float peso_factor = -479333; // calibrar

static void peso_task(void *arg)
{
    sensor_msg_t msg;

    while (1)
    {

        int32_t raw = hx711_read();
        float peso = (raw - peso_offset) / peso_factor;
        if (peso <= 0)
            peso = 0;
        msg.sensor = SENSOR_PESO;
        msg.valor1 = peso;
        msg.valor2 = 0;
        msg.timestamp_ms = esp_timer_get_time() / 1000;

        xQueueSend(sensor_queue, &msg, 0);

        ESP_LOGI("PESO", "Enviando Queue Peso: %.2f kg (raw=%ld)", peso, raw);

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

static void dht11_task(void *arg)
{
    sensor_msg_t msg;

    while (1)
    {
        if (dht11_read(&dht11_sensor, CONNECTION_TIMEOUT) == 0)
        {

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

static void suelo_task(void *arg)
{
    sensor_msg_t msg;
    int adc_raw;

    while (1)
    {

        adc_oneshot_read(adc_handle, SUELO_ADC_CHANNEL, &adc_raw);

        float humedad = 100.0 *
                        (SUELO_SECO - adc_raw) /
                        (SUELO_SECO - SUELO_MOJADO);

        if (humedad > 100)
            humedad = 100;
        if (humedad < 0)
            humedad = 0;

        msg.sensor = SENSOR_SUELO;
        msg.valor1 = humedad;
        msg.valor2 = 0;
        msg.timestamp_ms = esp_timer_get_time() / 1000;

        xQueueSend(sensor_queue, &msg, 0);

        ESP_LOGI("SUELO", "Enviando Queue Humedad: %.1f%% (raw=%d)", humedad, adc_raw);

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void sensores_init(void)
{
    // Inicializar ADC
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    adc_oneshot_new_unit(&init_config, &adc_handle);

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };

    // Se inicializa el sensor de peso.

    hx711_init(HX711_DOUT, HX711_SCK);

    // Calibrar offset inicial (sin peso)
    vTaskDelay(pdMS_TO_TICKS(1000));
    peso_offset = hx711_read();

    adc_oneshot_config_channel(adc_handle, SUELO_ADC_CHANNEL, &config);

    sensor_queue = xQueueCreate(QUEUE_LEN, sizeof(sensor_msg_t));
    if (!sensor_queue)
    {
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
        0);

    xTaskCreatePinnedToCore(
        suelo_task,
        "suelo_task",
        4096,
        NULL,
        5,
        NULL,
        0);

    xTaskCreatePinnedToCore(
        peso_task,
        "peso_task",
        4096,
        NULL,
        5,
        NULL,
        0);

    ESP_LOGI(TAG, "Queue y tareas inicializadas");
}

QueueHandle_t sensores_get_queue(void)
{
    return sensor_queue;
}
