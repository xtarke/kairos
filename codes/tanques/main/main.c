
/**
 * @file main.c
 * @brief Aquicontrol ESP32 RS-485 Sentral
 *
 *
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"

#include "esp_event_loop.h"
#include "esp_log.h"

#include "nvs.h"
#include "nvs_flash.h"


/* Custom includes */
#include "app_status.h"
#include "wifi_esptouch.h"
#include "pressure.h"
#include "mqtt.h"
#include "comm.h"

static const char *TAG = "INIT";

void app_main()
{
    // Initialize NVS (Non-Volatile Storage)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);

    configure_gpios();
    config_adc();
    initialise_wifi_touch();

    app_status_init();
    comm_init();

    mqtt_app_start();

    xTaskCreate(&adc_task, "adc_task", 512, NULL, 4, NULL);
    xTaskCreate(&pressure_task, "pressure_task", 1024, NULL, 4, NULL);    
    xTaskCreate(&status_task, "tank_status_task", 2048, NULL, 4, NULL);
    
}
