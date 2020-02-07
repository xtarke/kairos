
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
// #include "aws.h"

// #include "i2c_master.h"
// #include "uart.h"

// #include "Central.hpp"


void app_main()
{
    // Initialize NVS (Non-Volatile Storage)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // init_i2c_master_system();
    // init_uart1_system();
    // init_aws_system();

    configure_gpios();
    initialise_wifi_touch();

    app_status_init();

    // xTaskCreatePinnedToCore(&aws_iot_task, "aws_iot_task", 9216, NULL, 5, NULL, 1);
    // xTaskCreate(central_task, "main_app_task", 4096, NULL, 10, NULL);

    
}
