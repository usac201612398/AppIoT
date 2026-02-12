#include "esp_log.h"
#include "nvs_flash.h"    // Para nvs_flash_init()
#include "esp_netif.h"    // Para esp_netif_init()
#include "esp_event.h"    // Para esp_event_loop_create_default()

#include "sensores.h"
#include "listener.h"
#include "mqtt_service.h"
#include "wifi.h"     
#include "esp_log.h"

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_sta();
    mqtt_service_init();
    sensores_init();    
    listener_init();    
}