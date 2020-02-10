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

static void  status_publish_task(void *pvParameters)
{
	int msg_id;
    int len;
	TickType_t xLastWakeTime = xTaskGetTickCount();

	temperature_stauts_t myData = {0,0};
    publisher_data_t to_publish_data;
    publisher_data_t to_publish_pressure;

    memset(&to_publish_pressure, 0, sizeof(to_publish_pressure));
    memset(&to_publish_data, 0, sizeof(to_publish_data));

	esp_err_t err;
    nvs_handle my_handle;

    /* Open NVS (Non-Volatile Storage) */
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    
    if (err != ESP_OK) {
        ESP_LOGI(TAG,"Error (%s) opening NVS handle!\n", esp_err_to_name(err));
		strncpy(to_publish_data.topic,"host_1/temperatura/",PUB_TPC_LEN);
		strncpy(to_publish_pressure.topic,"host_1/pressao/",PUB_TPC_LEN);
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
		    strncpy(to_publish_data.topic,"host_1/temperatura/",PUB_TPC_LEN);
		    strncpy(to_publish_pressure.topic,"host_1/pressao/",PUB_TPC_LEN);   
        }
        
        nvs_close(my_handle);
    } 
  
    ESP_LOGI(TAG,"Topics are %s", to_publish_data.topic);
    while (1) {
        vTaskDelayUntil(&xLastWakeTime, 10000 / portTICK_PERIOD_MS);
        xEventGroupWaitBits(s_wifi_event_group, MQTT_CONNECTED_BIT, false, true, portMAX_DELAY);
       
        if (is_pressure_enable()){
			uint32_t pressure = get_pressure();
        	uint8_t error = get_pressure_error();

        	snprintf(to_publish_pressure.data, PUB_MSG_LEN, "%d.%d;%d", pressure/1000, pressure%1000, error);		
            msg_id = esp_mqtt_client_publish(client, to_publish_data.topic, to_publish_data.data, 0, 1, 0);
		}

        for (int i=0; i < MAX_485_SENSORS; i++){

        	if (get_enable(i)) {
        		len = strnlen(to_publish_data.topic, PUB_TPC_LEN);
        		to_publish_data.topic[len-1] = '0' + i;

        		get_temperature(i, &myData);

        		int fraction = myData.temperature % 10;
        		if (fraction < 0)
        			fraction = -fraction;

				snprintf(to_publish_data.data, PUB_MSG_LEN, "%d.%d;%d", myData.temperature/10, fraction, myData.error);
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
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            
            xEventGroupSetBits(s_wifi_event_group, MQTT_CONNECTED_BIT);
            
            // ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            // msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "data_3", 0, 1, 0);
            // ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

            // msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
            // ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            // msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
            // ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            // msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
            // ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_DISCONNECTED:
            xEventGroupClearBits(s_wifi_event_group, MQTT_CONNECTED_BIT);
            
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            //ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            break;
        case MQTT_EVENT_ERROR:
            xEventGroupClearBits(s_wifi_event_group, MQTT_CONNECTED_BIT);
            
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
    }
    return ESP_OK;
}

// static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
// {
//     /* For accessing reason codes in case of disconnection */
//     system_event_info_t *info = &event->event_info;
    
//     switch (event->event_id) {
//         case SYSTEM_EVENT_STA_START:
//             esp_wifi_connect();
//             break;
//         case SYSTEM_EVENT_STA_GOT_IP:
//             xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);

//             break;
//         case SYSTEM_EVENT_STA_DISCONNECTED:
//             ESP_LOGE(TAG, "Disconnect reason : %d", info->disconnected.reason);
//             if (info->disconnected.reason == WIFI_REASON_BASIC_RATE_NOT_SUPPORT) {
//                 /*Switch to 802.11 bgn mode */
//                 esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCAL_11B | WIFI_PROTOCAL_11G | WIFI_PROTOCAL_11N);
//             }
//             esp_wifi_connect();
//             xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
//             break;
//         default:
//             break;
//     }
//     return ESP_OK;
// }

// static void wifi_init(void)
// {
//     tcpip_adapter_init();
//     wifi_event_group = xEventGroupCreate();
//     ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));
//     wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
//     ESP_ERROR_CHECK(esp_wifi_init(&cfg));
//     ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
//     wifi_config_t wifi_config = {
//         .sta = {
//             .ssid = CONFIG_WIFI_SSID,
//             .password = CONFIG_WIFI_PASSWORD,
//         },
//     };
//     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
//     ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
//     ESP_LOGI(TAG, "start the WIFI SSID:[%s]", CONFIG_WIFI_SSID);
//     ESP_ERROR_CHECK(esp_wifi_start());
//     ESP_LOGI(TAG, "Waiting for wifi");
//     xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
// }

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
        // .user_context = (void *)your_context
    };

    ESP_LOGI(TAG, "Waiting for wifi");
    xEventGroupWaitBits(s_wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
    xEventGroupClearBits(s_wifi_event_group, MQTT_CONNECTED_BIT);

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(client);

    xTaskCreate(&status_publish_task, "status_publish_task", 1024, NULL, 6, NULL);

}

// void __app_main()
// {
//     nvs_flash_init();
//     wifi_init();
//     mqtt_app_start();
// }
