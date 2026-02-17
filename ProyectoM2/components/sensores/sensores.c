#include "sensores.h"
#include "sensor_msg.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "ultrasonic.h"
#include "ds18x20.h"
#include "onewire.h"

#define TRIG_PIN GPIO_NUM_5  // ultrasonico
#define ECHO_PIN GPIO_NUM_18 // ultrasonico

#define FLOW_SENSOR_PIN GPIO_NUM_4 // Pin de salida del sensor YF-S201
#define INTERVAL_MS 2000           // Intervalo de lectura (2s)
#define ML_PER_PULSE 2.25          // Mililitros por pulso (ajustable)
#define QUEUE_LEN 10

#define DS18B20_GPIO GPIO_NUM_15
#define MAX_DEVICES 1
#define ALTURA_TANQUE_CM 35.0
#define RELAY_ACTIVE_LEVEL 0   // 


static volatile uint32_t pulse_count = 0;
static portMUX_TYPE pulse_mux = portMUX_INITIALIZER_UNLOCKED;
static onewire_addr_t ds18x20_addrs[MAX_DEVICES];
static size_t ds18x20_device_count = 0;
static QueueHandle_t sensor_queue;

static const char *TAG = "SENSORES_TANQUE";

/* Descriptor del sensor */
static ultrasonic_sensor_t tank_sensor = {
    .trigger_pin = TRIG_PIN,
    .echo_pin = ECHO_PIN};


static void ultrasonico_task(void *arg)
{
    sensor_msg_t msg;
    uint32_t distancia_cm;

    while (1)
    {
        esp_err_t res = ultrasonic_measure_cm(&tank_sensor, 400, &distancia_cm);

        if (res == ESP_OK)
        {
            float nivel_cm = ALTURA_TANQUE_CM - distancia_cm;
            if (nivel_cm > ALTURA_TANQUE_CM)
                nivel_cm = ALTURA_TANQUE_CM;
                

            if (nivel_cm < 0)
                nivel_cm = 0;
            float porcentaje_llenado = 100 * (nivel_cm / ALTURA_TANQUE_CM);
            msg.sensor = SENSOR_ULTRASONICO;
            msg.valor1 = nivel_cm;
            msg.valor2 = porcentaje_llenado;
            msg.timestamp_ms = esp_timer_get_time() / 1000;

            xQueueSend(sensor_queue, &msg, portMAX_DELAY);

            ESP_LOGI(TAG, "Nivel: %.2f cm", nivel_cm);
            ESP_LOGI(TAG, "Nivel: %.2f %%", porcentaje_llenado);
        }
        else
        {
            ESP_LOGW(TAG, "Error medicion: %d", res);
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

// Interrupción: cada pulso incrementa el contador
static void IRAM_ATTR flow_pulse_isr(void *arg)
{
    pulse_count++;
}

// Tarea ajustada para recibir cola
static void caudal_task(void *arg)
{

    QueueHandle_t queue = (QueueHandle_t)arg;
    sensor_msg_t msg;

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(INTERVAL_MS));

        uint32_t count;
        portENTER_CRITICAL(&pulse_mux);
        count = pulse_count;
        pulse_count = 0;
        portEXIT_CRITICAL(&pulse_mux);

        float litros = count * ML_PER_PULSE / 1000.0;
        float minutos = INTERVAL_MS / 60000.0;
        float lpm = litros / minutos;

        msg.sensor = SENSOR_CAUDAL;
        msg.valor1 = lpm;
        msg.valor2 = 0;
        msg.timestamp_ms = esp_timer_get_time() / 1000;

        xQueueSend(queue, &msg, 0);

        ESP_LOGI(TAG, "Flujo: %.2f L/min (%d pulsos)", lpm, count);
    }
}

static void temperatura_task(void *arg)
{
    QueueHandle_t queue = (QueueHandle_t)arg;
    sensor_msg_t msg;

    while (1)
    {
        float temp;

        if (ds18x20_measure_and_read(DS18B20_GPIO,
                                     ds18x20_addrs[0],
                                     &temp) == ESP_OK)
        {
            msg.sensor = SENSOR_TEMP;
            msg.valor1 = temp;
            msg.valor2 = 0;
            msg.timestamp_ms = esp_timer_get_time() / 1000;

            xQueueSend(queue, &msg, 0);

            ESP_LOGI(TAG, "Temp: %.2f C", temp);
        }
        else
        {
            ESP_LOGE(TAG, "Error leyendo temperatura");
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void caudalimetro_init(QueueHandle_t queue)
{
    // No creamos la cola aquí, la recibimos como parámetro

    // Configuramos GPIO como input con pull-up
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_POSEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << FLOW_SENSOR_PIN),
        .pull_up_en = 1,
        .pull_down_en = 0};
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(FLOW_SENSOR_PIN, flow_pulse_isr, NULL);

    // Creamos la tarea que lee el flujo
    xTaskCreatePinnedToCore(
        caudal_task,
        "caudal_task",
        4096,
        (void *)queue, // pasamos la cola a la tarea
        5,
        NULL,
        0);
}

void temperatura_init(QueueHandle_t queue)
{
    ds18x20_scan_devices(DS18B20_GPIO,
                         ds18x20_addrs,
                         MAX_DEVICES,
                         &ds18x20_device_count);

    if (ds18x20_device_count == 0)
    {
        ESP_LOGE(TAG, "No se detectó DS18B20");
        return;
    }

    ESP_LOGI(TAG, "DS18B20 detectado");

    xTaskCreatePinnedToCore(
        temperatura_task,
        "temperatura_task",
        4096,
        (void *)queue,
        5,
        NULL,
        0);
}

void sensores_init(void)
{
    sensor_queue = xQueueCreate(QUEUE_LEN, sizeof(sensor_msg_t));

    caudalimetro_init(sensor_queue);
    temperatura_init(sensor_queue);
    ultrasonic_init(&tank_sensor);

    xTaskCreatePinnedToCore(
        ultrasonico_task,
        "ultrasonico_task",
        4096,
        NULL,
        5,
        NULL,
        0);
}

QueueHandle_t sensores_get_queue(void)
{
    return sensor_queue;
}
