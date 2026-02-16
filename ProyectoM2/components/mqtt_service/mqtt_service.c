#include "mqtt_service.h"
#include "esp_log.h"
#include "cJSON.h"
// #define BROKER "mqtt://10.111.112.4:1883"
// #define USER_MQTT "sdc-iot"
// #define PASS_MQTT "nuevacontraseña"
#define BROKER "mqtts://a4810e38lk0oy-ats.iot.us-east-1.amazonaws.com/8883"
#define TOPIC_PLANTA "casa/planta01/data"
#define TOPIC_LLENADO_MANUAL "casa/tanque01/llenado/manual"
#define TOPIC_RIEGO_MANUAL "casa/tanque01/riego/manual"
static planta_state_t planta_state = {0};
static const char aws_root_ca_pem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF\n"
    "ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6\n"
    "b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL\n"
    "MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv\n"
    "b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj\n"
    "ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM\n"
    "9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw\n"
    "IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6\n"
    "VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L\n"
    "93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm\n"
    "jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC\n"
    "AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA\n"
    "A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI\n"
    "U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs\n"
    "N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv\n"
    "o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU\n"
    "5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy\n"
    "rqXRfboQnoZsG4q5WTP468SQvvG5\n"
    "-----END CERTIFICATE-----\n";

static const char private_key_pem[] =
    "-----BEGIN RSA PRIVATE KEY-----\n"
    "MIIEpgIBAAKCAQEAtmQ4u3dQjgcfhDSAegjHrNpuAovi4l92GXhipdMfcdEr1N4N\n"
    "O4I85xbuSzZoD8pelUIW7JKWMA71rCKqmrIyTCunQw5+H3gcn06ojA7RxAxCvhJw\n"
    "ysIzwnIO+9lNBqEff1CY34FZVzhQ0yrVtBkoAN+oibrFggjnVjKM4ccRmBdw6p4/\n"
    "+WHstovKyiN0K4YzaYONKNS9xvrX5FMXaw2D3nq8FTM743CQgVpKcBYIwiHjx7e8\n"
    "H1p83HhH2NNKWseBphsaelyh8ZjwNjtZXjdfIU4fuR739t4mtOVbbp9OCwFD1S0+\n"
    "UNSNq6+xfpHybJ1TbXK3V3BYbgjgoU/ESxFBPwIDAQABAoIBAQCTWbCDs0b/Fz4c\n"
    "/pV1AbbfLLCHmh+4JsswlJONyFy1BPnWRXaHRxaQ03O8i73SU/nJt9TxVxPCy7Mq\n"
    "V+9gfuono3TDteeq4Myu30tHq4lIS2d4S0mYZQCP7LmyOcICwxTBNInst4FH95VE\n"
    "pGx1zYUF/6sXai66eRr8BmbO2JacOaa3tlkvw19WfgEwGR0M3/mCUNaa6ogPfuPe\n"
    "pqW6wFwu5JqtFNeylu5zf1BWm24EH7HWObQqmh6ALKzTRFbma7e9z0N2G6Fs2hZ5\n"
    "nk3ao9gh/og7oBYhsAORKexbtl2qJ7PoEHAW8Lm/GMKt5WazEeVfu82pdw3sdHLR\n"
    "na5lS77RAoGBANkcrNaKdudb/Zm1VCRhytAGIa4k2ziX0yvC2DAzJ2oK/sIlbBWr\n"
    "aIEuaHAssuwmni89jHhm9wwqju3itERVmUqqU+Wpw0f/Z39b1tCO5K2XZqLT+Si1\n"
    "kOX5ZttMPg8CoSHzvjVwuiOl1B1DaC4IE1R+2AqdpsuUrZLNM302M0GDAoGBANcP\n"
    "fgqqG1jvJsA1uc8nmJBadvUc+fXaF03C36zBSwPPhAOtEzOuvXzY0g7DZxy5xiZK\n"
    "gJ8rjzs6EoW9I3iUbDM+UGu0w0iiT0lSlwP8FAvINNHRfI/5ZKsIEz2a2Ryaq3kK\n"
    "HV5r1/7lnV8b/uB2j7zgYh3QHMZwCVBLTaTluWCVAoGBAJ4YHVtUFGVAPRhyS8T6\n"
    "WN7FuDzApck99Q1Gonnmpeq3+u6QpXXaDQ0UKASW5+rB7CnmHaWHBJodW1qp3C5u\n"
    "TmNqSBFSXtrMhibdTz0q8CXfu2MSC7qzC8IKq/VAPWhct8yrWnQ7k69lj2GHthLe\n"
    "2oSKoPKJ/ez8ZLAjMD0a/JPVAoGBALV5ZzyMioMjWym6rE539S9qhxMTHoItRJjj\n"
    "pmdavHg8tgN8GsXz4AXn2GmIzgvZX7GUELE/yp+Jw3ODRNlNNXiQTsB0So2SGSGg\n"
    "RbqhDjFbAedDxL6hDiecqs/DSQ3wHl4HAP1aN2vqSj+lmg/DPEt/dIv/vyhcBh/x\n"
    "S1WoXCN5AoGBAIVpBwllMTD+jwEEOPviX91VH1DUXEsXSOMcV5mnrIL9sUMOp0jC\n"
    "HxU2T2fDHhpxNDZHUhn/Y4bBk1j4jnP3W1VOCKuPeNH9XgaysklsqXR900aNBjkP\n"
    "/YdpO9pCWq1ZjVnNkUg58so/RYZJo/eVYGm+EtltbIBF+mTeRG1gNilz\n"
    "-----END RSA PRIVATE KEY-----\n";
static const char device_cert_pem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDWTCCAkGgAwIBAgIUSNU7rodIh+zLmiaMnbHQlJ619HowDQYJKoZIhvcNAQEL\n"
    "BQAwTTFLMEkGA1UECwxCQW1hem9uIFdlYiBTZXJ2aWNlcyBPPUFtYXpvbi5jb20g\n"
    "SW5jLiBMPVNlYXR0bGUgU1Q9V2FzaGluZ3RvbiBDPVVTMB4XDTI1MTIxOTE3MDgw\n"
    "N1oXDTQ5MTIzMTIzNTk1OVowHjEcMBoGA1UEAwwTQVdTIElvVCBDZXJ0aWZpY2F0\n"
    "ZTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALZkOLt3UI4HH4Q0gHoI\n"
    "x6zabgKL4uJfdhl4YqXTH3HRK9TeDTuCPOcW7ks2aA/KXpVCFuySljAO9awiqpqy\n"
    "Mkwrp0MOfh94HJ9OqIwO0cQMQr4ScMrCM8JyDvvZTQahH39QmN+BWVc4UNMq1bQZ\n"
    "KADfqIm6xYII51YyjOHHEZgXcOqeP/lh7LaLysojdCuGM2mDjSjUvcb61+RTF2sN\n"
    "g956vBUzO+NwkIFaSnAWCMIh48e3vB9afNx4R9jTSlrHgaYbGnpcofGY8DY7WV43\n"
    "XyFOH7ke9/beJrTlW26fTgsBQ9UtPlDUjauvsX6R8mydU21yt1dwWG4I4KFPxEsR\n"
    "QT8CAwEAAaNgMF4wHwYDVR0jBBgwFoAUXjI1ntra/x3wLUdscAtpdpyDij8wHQYD\n"
    "VR0OBBYEFD+G0h6Ii3r6mIkt2k8kvv2+kwRVMAwGA1UdEwEB/wQCMAAwDgYDVR0P\n"
    "AQH/BAQDAgeAMA0GCSqGSIb3DQEBCwUAA4IBAQA4rfUlVenjbteBhPAet7pEFzNk\n"
    "ttzbY8ATIG47lMnja3eEx22fDnskt86FaWLqVPN+wp9DOgIo2xQaeh3IShkmdFte\n"
    "at+LOmpp3BcdTYAUMQYWnGOeDVUUtTL0bAUCjzJDi2hpezWqgPi+9ZvsNcbshTq+\n"
    "aVEUXiNH8FSklNhNHFoI64LTGlbkjQt3dUNQljnpWHaq3MplfMZQl1skIIeO1+a+\n"
    "n8KaqPtMQJolLrK3Ni3d3IF2jsq1vQzu4qw8feFaM/FvgNUgfpjQTUY3mH0S083a\n"
    "O4DKEADMVlHpCPkrdFxOG/CWlugLs50N9MRRS9YUPH+QuEWvzb6m+UJ/zH15\n"
    "-----END CERTIFICATE-----\n";

static const char *TAG = "MQTT_SERVICE";
static esp_mqtt_client_handle_t client;
static bool mqtt_connected = false;
static void mqtt_handle_preparar_data(esp_mqtt_event_handle_t event);
static void mqtt_parse_planta_data(const char *data);

static void mqtt_parse_planta_data(const char *data)
{
    cJSON *root = cJSON_Parse(data);
    if (!root)
    {
        ESP_LOGW(TAG, "JSON invalido");
        return;
    }

    cJSON *temp = cJSON_GetObjectItem(root, "temp_amb");
    cJSON *hum = cJSON_GetObjectItem(root, "hum_amb");
    cJSON *suelo = cJSON_GetObjectItem(root, "hum_suelo");
    cJSON *peso = cJSON_GetObjectItem(root, "peso");

    if (temp && hum && suelo && peso)
    {
        planta_state.temp_amb = temp->valuedouble;
        planta_state.hum_amb = hum->valuedouble;
        planta_state.hum_suelo = suelo->valuedouble;
        planta_state.peso = peso->valuedouble;
        ESP_LOGI(TAG,
                 "Planta -> T:%.2f H:%.2f S:%.2f P:%.2f",
                 temp->valuedouble,
                 hum->valuedouble,
                 suelo->valuedouble,
                 peso->valuedouble);
    }

    cJSON_Delete(root);
}

static void mqtt_parse_manual(const char *topic, const char *data)
{
    cJSON *root = cJSON_Parse(data);
    if (!root)
        return;

    cJSON *accion = cJSON_GetObjectItem(root, "accion");
    ESP_LOGI(TAG, "mqtt_parse_manual: accion recibida = '%s'", accion->valuestring);
    if (accion && cJSON_IsString(accion))
    {
        if (strcmp(topic, TOPIC_LLENADO_MANUAL) == 0)
        {
            listener_manual_llenado(accion->valuestring);
        }
        else if (strcmp(topic, TOPIC_RIEGO_MANUAL) == 0)
        {
            listener_manual_riego(accion->valuestring);
        }
    }

    cJSON_Delete(root);
}

static void mqtt_handle_preparar_data(esp_mqtt_event_handle_t event)
{
    char topic[event->topic_len + 1];
    memcpy(topic, event->topic, event->topic_len);
    topic[event->topic_len] = 0;

    char data[event->data_len + 1];
    memcpy(data, event->data, event->data_len);
    data[event->data_len] = 0;

    ESP_LOGI(TAG, "RX topic: %s", topic);

    if (strcmp(topic, TOPIC_PLANTA) == 0)
    {
        mqtt_parse_planta_data(data);
    }
    else if (strcmp(topic, TOPIC_LLENADO_MANUAL) == 0 ||
             strcmp(topic, TOPIC_RIEGO_MANUAL) == 0)
    {
        mqtt_parse_manual(topic, data);
    }
}

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
        esp_mqtt_client_subscribe(client, TOPIC_PLANTA, 1);
        esp_mqtt_client_subscribe(client, TOPIC_LLENADO_MANUAL, 1);
        esp_mqtt_client_subscribe(client, TOPIC_RIEGO_MANUAL, 1);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT desconectado");

        mqtt_connected = false;
        break;
    case MQTT_EVENT_DATA:
        mqtt_handle_preparar_data(event);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT EVENT ERROR type=%d", event->error_handle->error_type);
        break;
    default:
        break;
    }
}

void mqtt_service_init(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = BROKER,
        .broker.verification.certificate = (const char *)aws_root_ca_pem,
        .credentials.authentication.certificate = (const char *)device_cert_pem,
        .credentials.authentication.key = (const char *)private_key_pem
        //.credentials.username = USER_MQTT,
        //.credentials.authentication.password = PASS_MQTT,
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

void mqtt_publicar_estado_valvula(bool abierta, float porcentaje)
{
    if (!mqtt_is_connected())
        return;

    cJSON *root = cJSON_CreateObject();
    if (!root)
        return;

    cJSON_AddStringToObject(root, "estado", abierta ? "llenando" : "cerrada");
    cJSON_AddNumberToObject(root, "porcentaje", porcentaje);

    char *json = cJSON_PrintUnformatted(root);
    if (json)
    {
        esp_mqtt_client_publish(
            mqtt_get_client(),
            "casa/tanque01/state",
            json,
            0,
            1,
            1); // retain activado

        free(json);
    }

    cJSON_Delete(root);
}

planta_state_t mqtt_get_planta_state(void)
{
    return planta_state;
}

void mqtt_publicar_estado_riego(bool activo)
{
    if (!mqtt_is_connected())
        return;

    cJSON *root = cJSON_CreateObject();
    if (!root)
        return;

    cJSON_AddStringToObject(root, "estado", activo ? "regando" : "detenido");

    char *json = cJSON_PrintUnformatted(root);
    if (json)
    {
        esp_mqtt_client_publish(
            mqtt_get_client(),
            "casa/riego/state",
            json,
            0,
            1,
            1); // retain activado
        free(json);
    }

    cJSON_Delete(root);
}