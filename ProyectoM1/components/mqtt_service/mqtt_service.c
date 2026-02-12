#include "mqtt_service.h"
#include "esp_log.h"

static const char *TAG = "MQTT_SERVICE";

static esp_mqtt_client_handle_t client;
static bool mqtt_connected = false;

static void mqtt_event_handler(void *handler_args,
                               esp_event_base_t base,
                               int32_t event_id,
                               void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id)
    {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT conectado");
            mqtt_connected = true;
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT desconectado");
            mqtt_connected = false;
            break;

        default:
            break;
    }
}

void mqtt_service_init(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://10.111.112.4:1883",
        .credentials.username = "sdc-iot",
        .credentials.authentication.password = "nuevacontraseña",
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

esp_mqtt_client_handle_t mqtt_get_client(void)
{
    return client;
}

bool mqtt_is_connected(void)
{
    return mqtt_connected;
}

