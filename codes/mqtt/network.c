/*
 * network.c
 *
 *  Created on: May 2, 2018
 *      Author: Renan Augusto Starke
 */

#include "espressif/esp_common.h"
#include "esp8266.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <ssid_config.h>

#include <espressif/esp_sta.h>
#include <espressif/esp_wifi.h>
#include <paho_mqtt_c/MQTTESP8266.h>
#include <paho_mqtt_c/MQTTClient.h>

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

#include "sysparam.h"

#include "network.h"
#include "comm.h"
#include "motors.h"

/* You can use http://test.mosquitto.org/ to test mqtt_client instead
 * of setting up your own MQTT server */
#define MQTT_PORT 1883

#define MQTT_USER NULL
#define MQTT_PASS NULL

//#define DEBUG

#ifdef DEBUG
#define debug(fmt, ...) printf("%s: " fmt "\n", "PWM", ## __VA_ARGS__)
#else
#define debug(fmt, ...)
#endif

QueueHandle_t publish_queue;

static const char *  get_my_id(void)
{
    // Use MAC address for Station as unique ID
    static char my_id[13];
    static bool my_id_done = false;
    int8_t i;
    uint8_t x;
    if (my_id_done)
        return my_id;
    if (!sdk_wifi_get_macaddr(STATION_IF, (uint8_t *)my_id))
        return NULL;
    for (i = 5; i >= 0; --i)
    {
        x = my_id[i] & 0x0F;
        if (x > 9) x += 7;
        my_id[i * 2 + 1] = x + '0';
        x = my_id[i] >> 4;
        if (x > 9) x += 7;
        my_id[i * 2] = x + '0';
    }
    my_id[12] = '\0';
    my_id_done = true;
    return my_id;
}

static void  topic_received(mqtt_message_data_t *md)
{
    int i;
    mqtt_message_t *message = md->message;
    printf("Received: ");
    for( i = 0; i < md->topic->lenstring.len; ++i)
        printf("%c", md->topic->lenstring.data[ i ]);

    printf(" = ");
    for( i = 0; i < (int)message->payloadlen; ++i)
        printf("%c", ((char *)(message->payload))[i]);

    printf("\r\n");
}



void  mqtt_task(void *pvParameters)
{
    int ret         = 0;
    struct mqtt_network network;
    mqtt_client_t client   = mqtt_client_default;
    char mqtt_client_id[20];
    uint8_t mqtt_buf[100];
    uint8_t mqtt_readbuf[100];
    mqtt_packet_connect_data_t data = mqtt_packet_connect_data_initializer;

    mqtt_network_new( &network );
    memset(mqtt_client_id, 0, sizeof(mqtt_client_id));
    strcpy(mqtt_client_id, "ESP-");
    strcat(mqtt_client_id, get_my_id());

    while(1) {
		char *wifi_mqtt_ip_addr = NULL;

		/* Get MQTT host IP from web server confifguration */
    	sysparam_get_string("wifi_mqtt_ip_addr", &wifi_mqtt_ip_addr);

    	if (wifi_mqtt_ip_addr) {
			printf("My mqtt server is: %s\n", wifi_mqtt_ip_addr);
			printf("%s: started\n\r", __func__);
			printf("%s: (Re)connecting to MQTT server %s ... ",__func__,
					wifi_mqtt_ip_addr);

    	}
    	else {
    		printf("%s: MQTT server configuration is invalid.\n",__func__);
    		printf("Retry in 10s\n\r");
			vTaskDelay(10000 / portTICK_PERIOD_MS);
    		continue;
    	}

        ret = mqtt_network_connect(&network, wifi_mqtt_ip_addr, MQTT_PORT);
        if( ret ){
            printf("error: %d. Retry in 10s\n\r", ret);
            vTaskDelay(10000 / portTICK_PERIOD_MS);
            continue;
        }

        mqtt_client_new(&client, &network, 5000, mqtt_buf, 100,
                      mqtt_readbuf, 100);

        data.willFlag       = 0;
        data.MQTTVersion    = 3;
        data.clientID.cstring   = mqtt_client_id;
        data.username.cstring   = MQTT_USER;
        data.password.cstring   = MQTT_PASS;
        data.keepAliveInterval  = 10;
        data.cleansession   = 0;

        ret = mqtt_connect(&client, &data);
        if(ret){
            printf("mqtt_connect error: %d\n\r", ret);
            mqtt_network_disconnect(&network);
            taskYIELD();
            continue;
        }
        printf("done\r\n");
        //mqtt_subscribe(&client, "/esptopic", MQTT_QOS1, topic_received);

        /* Subscribe to control topics *
		* #define MQTT_MAX_MESSAGE_HANDLERS in paho_mqtt_c/MQTTClient.h changed to 8  *
		* Default of only 5 handlers */
//        for (int i=0; i < NTOPICS; i++){
//			ret =  mqtt_subscribe(&client, moveTopcis[i], MQTT_QOS1, move_servo);
//
//			if (ret != MQTT_SUCCESS)
//				printf("Error on subscription: %d -> %d\n\n", i, ret);
//		   }
	    printf("Subscription ... done\r\n");

        xQueueReset(publish_queue);

        while(1){

            //char msg[PUB_MSG_LEN - 1] = "\0";

        	publisher_data_t to_publish;

            while(xQueueReceive(publish_queue, (void *)&to_publish, 0) ==
                  pdTRUE){
                //printf("got message to publish\r\n");
                mqtt_message_t message;
                message.payload = to_publish.data;
                message.payloadlen = PUB_MSG_LEN;
                message.dup = 0;
                message.qos = MQTT_QOS1;
                message.retained = 0;
                ret = mqtt_publish(&client, to_publish.topic, &message);
                if (ret != MQTT_SUCCESS ){
                    printf("error while publishing message: %d\n", ret );
                    break;
                }
            }

            ret = mqtt_yield(&client, 1000);
            if (ret == MQTT_DISCONNECTED)
                break;
        }
        printf("Connection dropped, request restart\n\r");
        mqtt_network_disconnect(&network);
        taskYIELD();
    }
}


