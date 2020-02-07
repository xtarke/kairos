/*
 *  wifi_esptouch.cpp
 *
 *  based on: Esptouch example
 *  This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 *  Unless required by applicable law or agreed to in writing, this
 *  software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 *  CONDITIONS OF ANY KIND, either express or implied.
 *      
 *  Modifications on: Dec 23, 2019
 *      Author: Renan Augusto Starke
 *
 *      Instituto Federal de Santa Catarina
 */

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_wpa2.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "tcpip_adapter.h"
#include "esp_smartconfig.h"
#include "driver/gpio.h"

#define LED_HARTBEAT (4)

/* FreeRTOS event group to signal when we are connected & ready to make a request */
EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;
static const int ESPTOUCH_DONE_BIT = BIT1;
static const char *TAG = "sc";

/* Number of retries after SYSTEM_EVENT_STA_DISCONNECTED */
#define MAXIMUM_RETRY  (5)
static int s_retry_num = 0;

void smartconfig_task(void * parm);
static void  hearbeat_task(void *pvParameters);

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    esp_err_t err;

    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        ESP_LOGI(TAG,"SYSTEM_EVENT_STA_START");
        
        err = esp_wifi_connect();
        ESP_LOGI(TAG,"%s in SYSTEM_EVENT_STA_START handle!\n", esp_err_to_name(err));

        /* SSID not configured: run ESP touch */
        if (err == ESP_ERR_WIFI_SSID){
            xTaskCreate(smartconfig_task, "smartconfig_task", 4096, NULL, 3, NULL);
        }
        break;
        
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG,"ESP connected\n");
        xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT);
        s_retry_num = 0;
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        
        if (s_retry_num < MAXIMUM_RETRY) {
                err = esp_wifi_connect();
                xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT);
                s_retry_num++;
                ESP_LOGI(TAG,"retry to connect to the AP: %s", esp_err_to_name(err));
        }
        ESP_LOGI(TAG,"connect to the AP fail\n");

        /*ToDo: use a button to reconfigure wifi */
        break;
    default:
        break;
    }
    return ESP_OK;
}

void initialise_wifi_touch(void)
{
    tcpip_adapter_init();
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_start() );

     xTaskCreate(&hearbeat_task, "led_task",  256, NULL, 3, NULL);
}

static void sc_callback(smartconfig_status_t status, void *pdata)
{
    switch (status) {
        case SC_STATUS_WAIT:
            ESP_LOGI(TAG, "SC_STATUS_WAIT");
            break;
        case SC_STATUS_FIND_CHANNEL:
            ESP_LOGI(TAG, "SC_STATUS_FINDING_CHANNEL");
            break;
        case SC_STATUS_GETTING_SSID_PSWD:
            ESP_LOGI(TAG, "SC_STATUS_GETTING_SSID_PSWD");
            break;
        case SC_STATUS_LINK:
            ESP_LOGI(TAG, "SC_STATUS_LINK");
            wifi_config_t *wifi_config = pdata;
            ESP_LOGI(TAG, "SSID:%s", wifi_config->sta.ssid);
            ESP_LOGI(TAG, "PASSWORD:%s", wifi_config->sta.password);

            esp_wifi_set_storage(WIFI_STORAGE_FLASH);

            ESP_ERROR_CHECK( esp_wifi_disconnect() );
            ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, wifi_config) );
            ESP_ERROR_CHECK( esp_wifi_connect() );
            break;
        case SC_STATUS_LINK_OVER:
            ESP_LOGI(TAG, "SC_STATUS_LINK_OVER");
            if (pdata != NULL) {
                uint8_t phone_ip[4] = { 0 };
                memcpy(phone_ip, (uint8_t* )pdata, 4);
                ESP_LOGI(TAG, "Phone ip: %d.%d.%d.%d\n", phone_ip[0], phone_ip[1], phone_ip[2], phone_ip[3]);
            }
            xEventGroupSetBits(s_wifi_event_group, ESPTOUCH_DONE_BIT);
            break;
        default:
            break;
    }
}

void configure_gpios(void){
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO15/16
    io_conf.pin_bit_mask = ((1ULL << LED_HARTBEAT));
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    gpio_set_level(LED_HARTBEAT, 1);
}

static void  hearbeat_task(void *pvParameters)
{
    EventBits_t uxBits;

    while(1) {
        uxBits = xEventGroupGetBits(s_wifi_event_group);

        if(uxBits & CONNECTED_BIT){
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

void smartconfig_task(void * parm)
{
    EventBits_t uxBits;
    ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH) );
    ESP_ERROR_CHECK( esp_smartconfig_start(sc_callback) );
    while (1) {
        uxBits = xEventGroupWaitBits(s_wifi_event_group, CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY); 
        if(uxBits & CONNECTED_BIT) {
            ESP_LOGI(TAG, "WiFi Connected to ap");
        }
        if(uxBits & ESPTOUCH_DONE_BIT) {
            ESP_LOGI(TAG, "smartconfig over");
            esp_smartconfig_stop();
            vTaskDelete(NULL);
        }
    }
}