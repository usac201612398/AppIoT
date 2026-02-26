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
#include <stdbool.h> 

#define PIN_VALVULA_LLENADO 27
#define PIN_BOMBA 32

#define PIN_VALVULA_ZONA1 25
#define PIN_VALVULA_ZONA2 33

#define NUM_ZONAS 2

#define NIVEL_MIN 20.0
#define NIVEL_MAX 80.0

#define TIEMPO_RIEGO_MS 20000

typedef struct
{
    bool activa;
    bool manual;
    float litros_acumulados;
    TickType_t fin_riego;
} zona_riego_t;

typedef enum
{
    CMD_NONE = 0,
    CMD_LLENADO_ON,
    CMD_LLENADO_OFF,
    CMD_LLENADO_AUTO
} listener_cmd_t;

static const char *TAG = "LISTENER";
// static QueueHandle_t cmd_queue;

static tanque_state_t tanque_state = {0};
static zona_riego_t zonas[NUM_ZONAS] = {0};

static int pines_zonas[NUM_ZONAS] = {
    PIN_VALVULA_ZONA1,
    PIN_VALVULA_ZONA2};

static bool bomba_activa = false;
static bool valvula_llenado_activa = false;
static bool tanque_manual = false;

/*
   CONTROL BOMBA
*/
static void actualizar_bomba()
{
    bool alguna_activa = false;

    for (int i = 0; i < NUM_ZONAS; i++)
    {
        if (zonas[i].activa)
        {
            alguna_activa = true;
            break;
        }
    }

    // Verificamos si el tanque tiene suficiente agua (porcentaje de llenado > mínimo)
    /*
    if (tanque_state.porcentaje_llenado < 5.0) // Umbral mínimo
    {
        ESP_LOGW(TAG, "El tanque está vacío o casi vacío. No se activará la bomba.");
        if (bomba_activa)
        {
            gpio_set_level(PIN_BOMBA, 1);
            bomba_activa = false;
            ESP_LOGW(TAG, "Bomba apagada por nivel bajo de tanque");
        }
        return; // No activar la bomba si el nivel es bajo
    }
    */
    // Verificación con el sensor ultrasonico
    if (tanque_state.nivel < 0.0 || tanque_state.nivel > 35.0)
    {
        ESP_LOGE(TAG, "Error: El sensor ultrasónico no está proporcionando datos válidos.");
        return; // No activar la bomba si el sensor falla
    }

    if (alguna_activa && !bomba_activa)
    {
        gpio_set_level(PIN_BOMBA, 0);
        bomba_activa = true;
        ESP_LOGI(TAG, "Bomba ON");
    }
    else if (!alguna_activa && bomba_activa)
    {
        gpio_set_level(PIN_BOMBA, 1);
        bomba_activa = false;
        ESP_LOGI(TAG, "Bomba OFF");
    }
}

/*
   INICIAR RIEGO
*/
static void iniciar_riego(int zona, int tiempo_ms)
{
    if (zona >= NUM_ZONAS)
        return;
    /*
    if (tanque_state.temp_agua > 45.0)

    {
        ESP_LOGW(TAG, "Riego bloqueado por temperatura alta del agua");
        return;
    }
    */
    gpio_set_level(pines_zonas[zona], 0);
    zonas[zona].activa = true;
    zonas[zona].fin_riego =
        xTaskGetTickCount() + pdMS_TO_TICKS(tiempo_ms);

    actualizar_bomba();

    ESP_LOGI(TAG, "Zona %d iniciada", zona + 1);
}

/*
CONTADOR DE LITROS
*/
static void publish_riego_resumen(int zona, float litros, bool manual)
{
    if (!mqtt_is_connected())
        return;

    cJSON *root = cJSON_CreateObject();
    if (!root)
        return;

    cJSON_AddNumberToObject(root, "zona", zona + 1);
    cJSON_AddNumberToObject(root, "litros_usados", litros);
    cJSON_AddBoolToObject(root, "manual", manual);

    char *json = cJSON_PrintUnformatted(root);

    if (json)
    {
        esp_mqtt_client_publish(
            mqtt_get_client(),
            "casa/tanque/1/riego_resumen",
            json,
            0,
            1,
            0);

        free(json);
    }

    cJSON_Delete(root);
}

/*
   DETENER RIEGO
*/
static void detener_riego(int zona)
{
    if (zona >= NUM_ZONAS)
        return;

    zonas[zona].activa = false;

    actualizar_bomba();
    gpio_set_level(pines_zonas[zona], 1);

    ESP_LOGI(TAG, "Zona %d detenida", zona + 1);
    ESP_LOGI(TAG, "Zona %d usó %.2f litros",
             zona + 1,
             zonas[zona].litros_acumulados);

    // Publicar resumen
    publish_riego_resumen(
        zona,
        zonas[zona].litros_acumulados,
        zonas[zona].manual);

    // Reset contador
    zonas[zona].litros_acumulados = 0;
}

/*
LLENADO MANUAL
*/
void listener_manual_llenado(const char *accion)
{
    if (strcmp(accion, "ON") == 0)
    {
        tanque_manual = true;
        valvula_llenado_activa = true;
        gpio_set_level(PIN_VALVULA_LLENADO, 1);
        ESP_LOGI(TAG, "Llenado manual ON");
    }
    else if (strcmp(accion, "OFF") == 0)
    {
        tanque_manual = true;
        valvula_llenado_activa = false;
        gpio_set_level(PIN_VALVULA_LLENADO, 0);
        ESP_LOGI(TAG, "Llenado manual OFF");
    }
    else if (strcmp(accion, "AUTO") == 0)
    {
        tanque_manual = false;
        ESP_LOGI(TAG, "Llenado modo AUTO");
    }
}

/*
   PUBLICAR TANQUE
*/
static void publish_tanque_state(void)
{
    if (!mqtt_is_connected())
        return;

    cJSON *root = cJSON_CreateObject();
    if (!root)
        return;

    cJSON_AddStringToObject(root, "tanque_id", "tanque_0001");
    cJSON_AddNumberToObject(root, "nivel", tanque_state.nivel);
    cJSON_AddNumberToObject(root, "caudal", tanque_state.caudal);
    cJSON_AddNumberToObject(root, "temp_agua", tanque_state.temp_agua);
    cJSON_AddNumberToObject(root, "porcentaje_llenado", tanque_state.porcentaje_llenado);

    char *json = cJSON_PrintUnformatted(root);

    if (json)
    {
        esp_mqtt_client_publish(
            mqtt_get_client(),
            "casa/tanque/1/data",
            json,
            0,
            1,
            0);

        free(json);
    }

    cJSON_Delete(root);
}

/*
RIEGO MANUAL
*/
void listener_riego_manual(int zona, const char *accion, int tiempo_ms)
{
    if (zona < 1 || zona > NUM_ZONAS)
        return;

    int index = zona - 1;
    publicar_historial_riego(zona, accion, tiempo_ms, true); // true = manual
    zonas[index].manual = true;

    if (strcmp(accion, "ON") == 0)
    {
        iniciar_riego(index, tiempo_ms);
    }
    else if (strcmp(accion, "OFF") == 0)
    {
        detener_riego(index);
    }
    else if (strcmp(accion, "AUTO") == 0)
    {
        zonas[index].manual = false;
    }
}

static void riego_automatico(int zona)
{
    if (zonas[zona].manual || zonas[zona].activa)
        return;
    /*
    if (tanque_state.temp_agua > 45.0)
    {
        ESP_LOGW(TAG, "Riego bloqueado por temperatura alta del agua");
        return;
    }
    */
    planta_state_t planta = mqtt_get_planta_state(zona); // zona debe ser una variable int porque asi se definio la estructura en el json 
    if (planta.temp_amb > 34.0 && planta.hum_amb < 34.0 && planta.hum_suelo < 15.0)
    {
        iniciar_riego(zona, TIEMPO_RIEGO_MS);
        publicar_historial_riego(zona + 1, "ON", TIEMPO_RIEGO_MS, false);
    }
}

/*
   LISTENER TASK
*/
static void listener_task(void *arg)
{
    QueueHandle_t q = sensores_get_queue();
    sensor_msg_t msg;

    TickType_t last_publish = xTaskGetTickCount();

    while (1)
    {
        /* PROCESAR SENSORES */
        if (xQueueReceive(q, &msg, pdMS_TO_TICKS(100)))
        {
            switch (msg.sensor)
            {
            case SENSOR_CAUDAL:
                tanque_state.caudal = msg.valor1;
                for (int i = 0; i < NUM_ZONAS; i++)
                {
                    if (zonas[i].activa)
                    {
                        float litros_intervalo = msg.valor1 * (2.0 / 60.0);
                        zonas[i].litros_acumulados += litros_intervalo;
                    }
                }
                break;

            case SENSOR_ULTRASONICO:
                tanque_state.nivel = msg.valor1;
                tanque_state.porcentaje_llenado = msg.valor2;

                if (!tanque_manual)
                {
                    if (!valvula_llenado_activa &&
                        tanque_state.porcentaje_llenado < NIVEL_MIN)
                    {
                        gpio_set_level(PIN_VALVULA_LLENADO, 1);
                        valvula_llenado_activa = true;  
                    }
                    else if (valvula_llenado_activa &&
                             tanque_state.porcentaje_llenado > NIVEL_MAX)
                    {
                        gpio_set_level(PIN_VALVULA_LLENADO, 0);
                        valvula_llenado_activa = false;
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
        TickType_t ahora = xTaskGetTickCount();

        /* CONTROL DE TIMEOUT ZONAS */
        for (int i = 0; i < NUM_ZONAS; i++)
        {
            if (zonas[i].activa &&
                (int32_t)(ahora - zonas[i].fin_riego) >= 0)
            {
                detener_riego(i);
            }
        }

        /* LÓGICA AUTOMÁTICA */
        // planta_state_t planta = mqtt_get_planta_state();

        for (int i = 0; i < NUM_ZONAS; i++)
        {
            riego_automatico(i);
        }
        /* PUBLICACIÓN PERIÓDICA */
        if ((int32_t)(ahora - last_publish) >= pdMS_TO_TICKS(10000))
        {
            publish_tanque_state();
            last_publish = ahora;
        }

        // vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* 
   INIT
 */
void listener_init(void)
{
    // cmd_queue = xQueueCreate(5, sizeof(listener_cmd_t));

    gpio_config_t io_conf = {
        .pin_bit_mask =
            (1ULL << PIN_VALVULA_LLENADO) |
            (1ULL << PIN_VALVULA_ZONA1) |
            (1ULL << PIN_VALVULA_ZONA2) |
            (1ULL << PIN_BOMBA),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};

    gpio_config(&io_conf);

    gpio_set_level(PIN_VALVULA_LLENADO, 1);
    gpio_set_level(PIN_VALVULA_ZONA1, 1);
    gpio_set_level(PIN_VALVULA_ZONA2, 1);
    gpio_set_level(PIN_BOMBA, 1);

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