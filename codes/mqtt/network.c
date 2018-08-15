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

#include "network.h"
#include "comm.h"
#include "motors.h"

/* You can use http://test.mosquitto.org/ to test mqtt_client instead
 * of setting up your own MQTT server */
#define MQTT_HOST ("192.168.0.113")
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
SemaphoreHandle_t wifi_alive;

#define NTOPICS 4

static char *moveTopcis[NTOPICS] = {"/robot/head/vert",
								"/robot/head/horz",
								"/robot/left/vert",
								"/robot/left/horz"};
/*
static char *moveTopcis[NTOPICS] = {"/robot/servos/mov/0",
								"/robot/servos/mov/1",
								"/robot/servos/mov/2",
								"/robot/servos/mov/3"};
*/


static void move_servo(mqtt_message_data_t *md){
	char msgString[4] = {0,0,0,0};
	uint8_t size;

	/* Pre-defined packge */
	uint8_t pkg[PWM_TOTAL_PKG_SIZE] = { 0, //Initializer
										SERVO_PAYLOAD_SIZE,
										PWM_DATA,
										0, //Servo Id
										0, //Servo pos
										0 }; //Checksum

	mqtt_message_t *message = md->message;
	
	/* Get servo id from topic */
	size = md->topic->lenstring.len;
	pkg[3] = md->topic->lenstring.data[size-1] - '0';

	/* Get servo position from payload */
	size = (message->payloadlen < 4) ? message->payloadlen : 4;
	memcpy(msgString, message->payload, size);
	msgString[3] = 0;
	pkg[4] = atoi(msgString);

	setPos(pkg[4], pkg[3]);

#ifdef DEBUG_MSGS
	debug("%u =  %u\n\r", pkg[3], pkg[4]);
#endif

	if (xQueueSend(tx_queue, (void *)pkg, 0) == pdFALSE) {
		debug("uart_queue overflow.\r\n");
	}

	xTaskNotify( xHandlingUartTask, 0, eNoAction );
}


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

void  wifi_task(void *pvParameters)
{
    uint8_t status  = 0;
    uint8_t retries = 30;
    struct sdk_station_config config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASS,
    };

    printf("WiFi: connecting to WiFi\n\r");
    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&config);

    while(1)
    {
        while ((status != STATION_GOT_IP) && (retries)){
            status = sdk_wifi_station_get_connect_status();
            printf("%s: status = %d\n\r", __func__, status );
            if( status == STATION_WRONG_PASSWORD ){
                printf("WiFi: wrong password\n\r");
                break;
            } else if( status == STATION_NO_AP_FOUND ) {
                printf("WiFi: AP not found\n\r");
                break;
            } else if( status == STATION_CONNECT_FAIL ) {
                printf("WiFi: connection failed\r\n");
                break;
            }
            vTaskDelay( 1000 / portTICK_PERIOD_MS );
            --retries;
        }
        if (status == STATION_GOT_IP) {
            printf("WiFi: Connected\n\r");
            xSemaphoreGive( wifi_alive );
            taskYIELD();
        }

        while ((status = sdk_wifi_station_get_connect_status()) == STATION_GOT_IP) {
            xSemaphoreGive( wifi_alive );
            taskYIELD();
        }
        printf("WiFi: disconnected\n\r");
        sdk_wifi_station_disconnect();
        vTaskDelay( 1000 / portTICK_PERIOD_MS );
    }
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
        xSemaphoreTake(wifi_alive, portMAX_DELAY);
        printf("%s: started\n\r", __func__);
        printf("%s: (Re)connecting to MQTT server %s ... ",__func__,
               MQTT_HOST);
        ret = mqtt_network_connect(&network, MQTT_HOST, MQTT_PORT);
        if( ret ){
            printf("error: %d\n\r", ret);
            taskYIELD();
            continue;
        }
        printf("done\n\r");
        mqtt_client_new(&client, &network, 5000, mqtt_buf, 100,
                      mqtt_readbuf, 100);

        data.willFlag       = 0;
        data.MQTTVersion    = 3;
        data.clientID.cstring   = mqtt_client_id;
        data.username.cstring   = MQTT_USER;
        data.password.cstring   = MQTT_PASS;
        data.keepAliveInterval  = 10;
        data.cleansession   = 0;
        printf("Send MQTT connect ... ");
        ret = mqtt_connect(&client, &data);
        if(ret){
            printf("error: %d\n\r", ret);
            mqtt_network_disconnect(&network);
            taskYIELD();
            continue;
        }
        printf("done\r\n");
        //mqtt_subscribe(&client, "/esptopic", MQTT_QOS1, topic_received);


        /* Subscribe to control topics *
		* #define MQTT_MAX_MESSAGE_HANDLERS in paho_mqtt_c/MQTTClient.h changed to 8  *
		* Default of only 5 handlers */
        for (int i=0; i < NTOPICS; i++){
			ret =  mqtt_subscribe(&client, moveTopcis[i], MQTT_QOS1, move_servo);

			if (ret != MQTT_SUCCESS)
				printf("Error on subscription: %d -> %d\n\n", i, ret);
		   }
	    printf("Subscription ... done\r\n");

        xQueueReset(publish_queue);
		
		//------------------------------------------------------------------
	/*
		mqtt_message_t message;

		char msg_1[] = "robot/head/vert";
		char msg_2[] = "robot/head/horz";
		char msg_3[] = "robot/left/vert";
		char msg_4[] = "robot/left/horz";

		uint8_t size = strlen(msg_1);

		message.payload = msg_1;
		message.payloadlen = size; // PUB_MSG_LEN;
		message.dup = 0;
		message.qos = MQTT_QOS1;
		message.retained = 0;

		// CONEXAO COM QT
		ret = mqtt_publish(&client, "robot", &message);
		if (ret != MQTT_SUCCESS ){
			printf("error while publishing message: %d\n", ret );
			break;
		}

		size = strlen(msg_2);

		message.payload = msg_2;
		message.payloadlen = size; // PUB_MSG_LEN;
		message.dup = 0;
		message.qos = MQTT_QOS1;
		message.retained = 0;
		
		ret = mqtt_publish(&client, "robot", &message);
		if (ret != MQTT_SUCCESS ){
			printf("error while publishing message: %d\n", ret );
			break;
		}

		size = strlen(msg_3);

		message.payload = msg_3;
		message.payloadlen = size; // PUB_MSG_LEN;
		message.dup = 0;
		message.qos = MQTT_QOS1;
		message.retained = 0;
		
		ret = mqtt_publish(&client, "robot", &message);
		if (ret != MQTT_SUCCESS ){
			printf("error while publishing message: %d\n", ret );
			break;
		}
		
		size = strlen(msg_4);

		message.payload = msg_4;
		message.payloadlen = size; // PUB_MSG_LEN;
		message.dup = 0;
		message.qos = MQTT_QOS1;
		message.retained = 0;
		
		ret = mqtt_publish(&client, "robot", &message);
		if (ret != MQTT_SUCCESS ){
			printf("error while publishing message: %d\n", ret );
			break;
		}
		*/
		//------------------------------------------------------------------
		
        while(1){

            char msg[PUB_MSG_LEN - 1] = "\0";
            while(xQueueReceive(publish_queue, (void *)msg, 0) ==
                  pdTRUE){
                //printf("got message to publish\r\n");
                mqtt_message_t message;
                message.payload = msg;
                message.payloadlen = PUB_MSG_LEN;
                message.dup = 0;
                message.qos = MQTT_QOS1;
                message.retained = 0;
                ret = mqtt_publish(&client, "/beat", &message);
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


