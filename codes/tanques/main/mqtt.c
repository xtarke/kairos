#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event_loop.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "wifi_esptouch.h"
#include "app_status.h"
#include "mqtt.h"

static const char *TAG = "MQTT";
esp_mqtt_client_handle_t client;

static void status_publish_task(void *pvParameters)
{
    int msg_id;
    int len;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    temperature_stauts_t myData = {0, 0};
    publisher_data_t to_publish_data;
    publisher_data_t to_publish_pressure;

    memset(&to_publish_pressure, 0, sizeof(to_publish_pressure));
    memset(&to_publish_data, 0, sizeof(to_publish_data));

    esp_err_t err;
    nvs_handle my_handle;

    ESP_LOGI(TAG, "Config host name is: %s", CONFIG_HOST_NAME);
    /* Open NVS (Non-Volatile Storage) */
    /* err = nvs_open("storage", NVS_READWRITE, &my_handle);

if (err != ESP_OK) {
ESP_LOGI(TAG,"Error (%s) opening NVS handle!", esp_err_to_name(err));
strncpy(to_publish_data.topic, CONFIG_HOST_NAME"/temperatrua/" ,PUB_TPC_LEN);
strncpy(to_publish_pressure.topic,CONFIG_HOST_NAME"/pressao/",PUB_TPC_LEN);
}
else {
size_t required_size;
err = nvs_get_str(my_handle, "hostname", NULL, &required_size);

if (err == ESP_OK){
char * wifi_my_host_name = malloc(required_size);
nvs_get_str(my_handle, "hostname", wifi_my_host_name, &required_size);

ESP_LOGI(TAG,"hostname is %s", wifi_my_host_name);

strncpy(to_publish_data.topic, wifi_my_host_name,PUB_TPC_LEN);
strncpy(to_publish_pressure.topic, wifi_my_host_name,PUB_TPC_LEN);
free(wifi_my_host_name);
strncat(to_publish_data.topic,"/temperatura/0",PUB_TPC_LEN);
strncat(to_publish_pressure.topic,"/pressao",PUB_TPC_LEN);

free(wifi_my_host_name);
}
else
{
ESP_LOGI(TAG, "Invalid host name\n");
strncpy(to_publish_data.topic,CONFIG_HOST_NAME"/temperatura/",PUB_TPC_LEN);
strncpy(to_publish_pressure.topic,CONFIG_HOST_NAME"/pressao/",PUB_TPC_LEN);
}

nvs_close(my_handle);
}*/

    /* ToDo: Use NVS to reconfigure and a service */
    strncpy(to_publish_data.topic, CONFIG_HOST_NAME "/temperatura/0", PUB_TPC_LEN);
    strncpy(to_publish_pressure.topic, CONFIG_HOST_NAME "/pressao/", PUB_TPC_LEN);
    ESP_LOGI(TAG, "Topics are %s", to_publish_data.topic);

    while (1)
    {
        vTaskDelayUntil(&xLastWakeTime, 10000 / portTICK_PERIOD_MS);
        xEventGroupWaitBits(s_wifi_event_group, MQTT_CONNECTED_BIT, false, true, portMAX_DELAY);

        if (is_pressure_enable())
        {
            uint32_t pressure = get_pressure();
            uint8_t error = get_pressure_error();

            snprintf(to_publish_pressure.data, PUB_MSG_LEN, "%d.%d;%d", pressure / 1000, pressure % 1000, error);
            msg_id = esp_mqtt_client_publish(client, to_publish_pressure.topic, to_publish_pressure.data, 0, 1, 0);
        }

        for (int i = 0; i < MAX_485_SENSORS; i++)
        {

            if (get_enable(i))
            {
                len = strnlen(to_publish_data.topic, PUB_TPC_LEN);
                to_publish_data.topic[len - 1] = '0' + i;

                get_temperature(i, &myData);

                int fraction = myData.temperature % 10;
                if (fraction < 0)
                    fraction = -fraction;

                snprintf(to_publish_data.data, PUB_MSG_LEN, "%d.%d;%d", myData.temperature / 10, fraction, myData.error);
                msg_id = esp_mqtt_client_publish(client, to_publish_data.topic, to_publish_data.data, 0, 1, 0);

                vTaskDelayUntil(&xLastWakeTime, 1000 / portTICK_PERIOD_MS);
            }
        }
    }
}

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    // your_context_t *context = event->context;

    char sub_topic[] = CONFIG_HOST_NAME "/config";

    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:

        xEventGroupSetBits(s_wifi_event_group, MQTT_CONNECTED_BIT);

        // ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        // msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "data_3", 0, 1, 0);
        // ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

        // msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
        // ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        ESP_LOGI(TAG, "set config topic=%s", sub_topic);
        msg_id = esp_mqtt_client_subscribe(client, sub_topic, 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        // msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
        // ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        xEventGroupClearBits(s_wifi_event_group, MQTT_CONNECTED_BIT);

        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        /* ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);*/
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        //ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        // printf("DATA=%.*s\r\n", event->data_len, event->data);
        switch (event->data[0])
        {
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
        break;
    case MQTT_EVENT_ERROR:
        xEventGroupClearBits(s_wifi_event_group, MQTT_CONNECTED_BIT);

        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    }
    return ESP_OK;
}

void mqtt_app_start(void)
{
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = CONFIG_BROKER_URL,
        .event_handle = mqtt_event_handler,
        .username = CONFIG_BROKER_USER,
        .password = CONFIG_BROKER_PASS
        // .user_context = (void *)your_context
    };

    ESP_LOGI(TAG, "Waiting for wifi");
    xEventGroupWaitBits(s_wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
    xEventGroupClearBits(s_wifi_event_group, MQTT_CONNECTED_BIT);

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(client);

    xTaskCreate(&status_publish_task, "status_publish_task", 2048, NULL, 6, NULL);
}
