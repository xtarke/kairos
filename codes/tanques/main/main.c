
/**
 * @file main.c
 * @brief Kairos ESP8266 
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
#include "driver/gpio.h"

/* Custom includes */
#include "app_status.h"
#include "wifi_esptouch.h"
#include "pressure.h"
#include "mqtt.h"
#include "comm.h"

#define LED_HARTBEAT (4)

static const char *TAG = "INIT";

static void  hearbeat_task(void *pvParameters)
{
  volatile EventBits_t uxBits;

  while(1) {
      uxBits = xEventGroupGetBits(s_wifi_event_group);

      if((uxBits & CONNECTED_BIT) | (uxBits & MQTT_CONNECTED_BIT)){
          gpio_set_level(LED_HARTBEAT, 1);
          vTaskDelay(1000 / portTICK_PERIOD_MS);
          gpio_set_level(LED_HARTBEAT, 0);
          vTaskDelay(1000 / portTICK_PERIOD_MS);
      } else
      {
          gpio_set_level(LED_HARTBEAT, 1);
          vTaskDelay(200 / portTICK_PERIOD_MS);
          gpio_set_level(LED_HARTBEAT, 0);
          vTaskDelay(200 / portTICK_PERIOD_MS);
      }
  }
}

void configure_gpios(void){
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins to set
    io_conf.pin_bit_mask = ((1ULL << LED_HARTBEAT) | (1ULL << TX_EN_PIN));
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    gpio_set_level(LED_HARTBEAT, 1);
}


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
    app_status_init();
    comm_init();
    initialise_wifi_touch();

    xTaskCreate(&hearbeat_task, "led_task",  256, NULL, 3, NULL);

    mqtt_app_start();

    xTaskCreate(&adc_task, "adc_task", 512, NULL, 4, NULL);
    xTaskCreate(&pressure_task, "pressure_task", 1024, NULL, 4, NULL);
    xTaskCreate(&status_task, "tank_status_task", 2048, NULL, 4, NULL);

}
