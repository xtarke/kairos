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

#include "driver/uart.h"
#include "app_status.h"

/* FreeRTOS event group to signal when we are connected & ready to make a request */
EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event,
    but we only care about one event - are we connected
    to the AP with an IP? */
const int CONNECTED_BIT = BIT0;
const int ESPTOUCH_DONE_BIT = BIT1;
const int MQTT_CONNECTED_BIT = BIT2;

static const char *TAG = "WIFI";

/* Number of retries after SYSTEM_EVENT_STA_DISCONNECTED */
#define MAXIMUM_RETRY (20)
static int s_retry_num = 0;

void smartconfig_task(void *parm);

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    int len = 1;
    unsigned char data[64];
    wifi_config_t cfg;
    esp_err_t err;

    switch (event->event_id)
    {
    case SYSTEM_EVENT_STA_START:

        err = esp_wifi_get_config(ESP_IF_WIFI_STA, &cfg);

        ESP_LOGI(TAG, "Wating 10s for configuration...");

        while (len != 0)
        {
            len = uart_read_bytes(UART_NUM_0, data, 1, 10000 / portTICK_RATE_MS);

            if (len != 0)
            {
                switch (data[0])
                {
                case 'w':
                    ESP_LOGI(TAG, "Wifi RESET...");
                    cfg.sta.ssid[0] = 0;
                    break;
                case '0':
                    ESP_LOGI(TAG, "Set/Unset sensor 0");
                    save_enable(0);
                    break;
                case '1':
                    ESP_LOGI(TAG, "Set/Unset sensor 1");
                    save_enable(1);
                    break;
                case '2':
                    ESP_LOGI(TAG, "Set/Unset sensor 2");
                    save_enable(2);
                    break;
                case '3':
                    save_enable(3);
                    ESP_LOGI(TAG, "Set/Unset sensor 3");
                    break;
                case 'p':
                    save_enable(4);
                    ESP_LOGI(TAG, "Set/Unset pressure sensor");
                    break;
                default:
                    break;
                }
            }
        }

        if (get_reset_wifi()) {
          ESP_LOGI(TAG, "Wifi RESET from MQTT...");
          set_reset_wifi(0);
          cfg.sta.ssid[0] = 0;
        }

        ESP_LOGI(TAG, "%s in esp_wifi_get_config", esp_err_to_name(err));

        ESP_LOGI(TAG, "SSID: %s", cfg.sta.ssid);
        ESP_LOGI(TAG, "PASS: %s", cfg.sta.password);

        err = esp_wifi_connect();

        /* SSID not configured: run ESP touch */
        if (cfg.sta.ssid[0] == 0)
        {
            xTaskCreate(smartconfig_task, "smartconfig_task", 4096, NULL, 3, NULL);
        }
        break;

    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "ESP connected\n");
        xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT);
        s_retry_num = 0;
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        if (s_retry_num < MAXIMUM_RETRY)
        {
            err = esp_wifi_connect();
            xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT);
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP: %s", esp_err_to_name(err));
        }
        else {
          ESP_LOGI(TAG, "Connect to the AP fail, Restarting now...\n");
          esp_restart();
        }
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
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void sc_callback(smartconfig_status_t status, void *pdata)
{
    switch (status)
    {
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

        ESP_ERROR_CHECK(esp_wifi_disconnect());
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, wifi_config));
        ESP_ERROR_CHECK(esp_wifi_connect());
        break;
    case SC_STATUS_LINK_OVER:
        ESP_LOGI(TAG, "SC_STATUS_LINK_OVER");
        if (pdata != NULL)
        {
            uint8_t phone_ip[4] = {0};
            memcpy(phone_ip, (uint8_t *)pdata, 4);
            ESP_LOGI(TAG, "Phone ip: %d.%d.%d.%d\n", phone_ip[0], phone_ip[1], phone_ip[2], phone_ip[3]);
        }
        xEventGroupSetBits(s_wifi_event_group, ESPTOUCH_DONE_BIT);
        break;
    default:
        break;
    }
}

void smartconfig_task(void *parm)
{
    EventBits_t uxBits;
    ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH));
    ESP_ERROR_CHECK(esp_smartconfig_start(sc_callback));
    while (1)
    {
        uxBits = xEventGroupWaitBits(s_wifi_event_group, CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY);
        if (uxBits & CONNECTED_BIT)
        {
            ESP_LOGI(TAG, "WiFi Connected to ap");
        }
        if (uxBits & ESPTOUCH_DONE_BIT)
        {
            ESP_LOGI(TAG, "smartconfig over");
            esp_smartconfig_stop();
            vTaskDelete(NULL);
        }
    }
}
