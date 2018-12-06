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
#include <ctype.h>
#include <ssid_config.h>

#include <espressif/esp_sta.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_common.h>
#include <espressif/user_interface.h>

#include <paho_mqtt_c/MQTTESP8266.h>
#include <paho_mqtt_c/MQTTClient.h>

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

#include <esp/uart.h>
#include <FreeRTOS.h>
#include <task.h>

#include "lwip/sockets.h"
#include "wificfg/wificfg.h"
#include "sysparam.h"

#include "network.h"
#include "comm.h"
#include "app_status.h"

/* You can use http://test.mosquitto.org/ to test mqtt_client instead
 * of setting up your own MQTT server */
#define MQTT_PORT 1883

#include "broker_credenctials.h"

//#define DEBUG

#ifdef DEBUG
#define debug(fmt, ...) printf(fmt, ## __VA_ARGS__)
#else
#define debug(fmt, ...)
#endif

QueueHandle_t publish_queue;
QueueHandle_t command_queue;

char cfg_topics[CFG_TOPICS][PUB_TPC_LEN];

void init_mqtt_topics(){

	char *wifi_my_host_name = NULL;
	char *default_host_name = "host_1";

	/* Get MQTT host IP from web server configuration */
	sysparam_get_string("hostname", &wifi_my_host_name);

	if (!wifi_my_host_name){
		debug("init_mqtt_topics: Invalid host name\n");
		wifi_my_host_name = default_host_name;
	}
	else {
		for (int i=0; i < CFG_TOPICS; i++) {
			snprintf(cfg_topics[i], PUB_TPC_LEN, "%s/cfg/%d", wifi_my_host_name,i);
			debug("%s\n", cfg_topics[i]);
		}
	}

	if (wifi_my_host_name)
		free(wifi_my_host_name);

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

static void topic_received(mqtt_message_data_t *md)
{
    int size;
	mqtt_message_t *message = md->message;
	command_data_t rx_data;

#ifdef DEBUG
    int i;
    debug("Received: ");
    for( i = 0; i < md->topic->lenstring.len; ++i)
    	debug("%c", md->topic->lenstring.data[ i ]);
    debug(" = ");
    for( i = 0; i < (int)message->payloadlen; ++i)
    	debug("%c", ((char *)(message->payload))[i]);
    debug("\r\n");
#endif

    /* Convert message data to numeric data    *
     * Command type from last char from topic  */
    size = md->topic->lenstring.len;
    rx_data.cmd = md->topic->lenstring.data[size-1] - '0';

    /* Ensure string termination  */
    size = message->payloadlen;
    ((char *)(message->payload))[size] = 0;
    /* Convert string to int */
    rx_data.data = atoi((char *)(message->payload));

    /* New rs485 address: set it and return immediately */
    if (rx_data.cmd == 0) {
    	set_rs485_addr((uint8_t)rx_data.data);
    	return;
    }

    /* Enqueue data and unblock task */
    if (xQueueSend(command_queue, (void *)&rx_data, 0) == pdFALSE)
		debug("rx_queue queue overflow.\r\n");

    xTaskNotify(xHandling_485_cmd_task, 0, eNoAction);
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


    uint8_t mode = sdk_wifi_get_opmode();

    while((mode != 3) && (mode != 1)){
    	debug("Wating WiFi config. Mode: %d\n", mode);
    	mode = sdk_wifi_get_opmode();
    	vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    mqtt_network_new( &network );
    memset(mqtt_client_id, 0, sizeof(mqtt_client_id));
    strcpy(mqtt_client_id, "ESP-");
    strcat(mqtt_client_id, get_my_id());

    init_mqtt_topics();

    while(1) {
		char *wifi_mqtt_ip_addr = NULL;

		/* Get MQTT host IP from web server confifguration */
    	sysparam_get_string("wifi_mqtt_ip_addr", &wifi_mqtt_ip_addr);

    	if (wifi_mqtt_ip_addr) {
    		debug("My mqtt server is: %s\n", wifi_mqtt_ip_addr);
    		debug("%s: started\n\r", __func__);
    		debug("%s: (Re)connecting to MQTT server %s ... ",__func__,
					wifi_mqtt_ip_addr);

    	}
    	else {
    		debug("%s: MQTT server configuration is invalid.\n",__func__);
    		debug("Retry in 10s\n\r");
			vTaskDelay(10000 / portTICK_PERIOD_MS);
    		continue;
    	}

        ret = mqtt_network_connect(&network, wifi_mqtt_ip_addr, MQTT_PORT);
        if( ret ){
        	debug("error: %d. Retry in 10s\n\r", ret);
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
        	debug("mqtt_connect error: %d\n\r", ret);
            mqtt_network_disconnect(&network);
            taskYIELD();
            continue;
        }

        /* Subscribe to control topics *
		* #define MQTT_MAX_MESSAGE_HANDLERS in paho_mqtt_c/MQTTClient.h changed to 8  *
		* Default of only 5 handlers */
        for (int i=0; i < CFG_TOPICS; i++){
        	ret =  mqtt_subscribe(&client, cfg_topics[i], MQTT_QOS1, topic_received);

        	if (ret != MQTT_SUCCESS)
				debug("Error on subscription: %d -> %d\n\n", i, ret);
		}

        debug("Subscription ... done\r\n");

        xQueueReset(publish_queue);

        while(1){
        	publisher_data_t to_publish;

            while(xQueueReceive(publish_queue, (void *)&to_publish, 0) == pdTRUE){
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

static const char http_success_header[] = "HTTP/1.1 200 \r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Cache-Control: no-store\r\n"
    "Transfer-Encoding: chunked\r\n"
    "Connection: close\r\n"
    "\r\n";
static const char *http_index_content[] = {
#include "content/index.html"
};

static int handle_index(int s, wificfg_method method,
                        uint32_t content_length,
                        wificfg_content_type content_type,
                        char *buf, size_t len)
{
    if (wificfg_write_string(s, http_success_header) < 0) return -1;

    if (method != HTTP_METHOD_HEAD) {
        if (wificfg_write_string_chunk(s, http_index_content[0], buf, len) < 0) return -1;
        if (wificfg_write_html_title(s, buf, len, "Home") < 0) return -1;
        if (wificfg_write_string_chunk(s, http_index_content[1], buf, len) < 0) return -1;

        if (wificfg_write_string_chunk(s, "<dl class=\"dlh\">", buf, len) < 0) return -1;

        struct sockaddr_storage addr;
        socklen_t addr_len = sizeof(addr);
        if (getpeername(s, (struct sockaddr *)&addr, &addr_len) == 0) {
            if (((struct sockaddr *)&addr)->sa_family == AF_INET) {
                struct sockaddr_in *sa = (struct sockaddr_in *)&addr;
                if (wificfg_write_string_chunk(s, "<dt>Peer address</dt>", buf, len) < 0) return -1;

                snprintf(buf, len, "<dd>" IPSTR ", port %u</dd>",
                         IP2STR((ip4_addr_t *)&sa->sin_addr.s_addr), ntohs(sa->sin_port));

                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
            }

#if LWIP_IPV6
            if (((struct sockaddr *)&addr)->sa_family == AF_INET6) {
                struct sockaddr_in6 *sa = (struct sockaddr_in6 *)&addr;
                if (wificfg_write_string_chunk(s, "<dt>Peer address</dt><dd>", buf, len) < 0) return -1;
                if (inet6_ntoa_r(sa->sin6_addr, buf, len)) {
                    if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
                }
                snprintf(buf, len, ", port %u</dd>", ntohs(sa->sin6_port));
                if (wificfg_write_string_chunk(s, buf, buf, len) < 0) return -1;
            }
#endif
        }

        if (wificfg_write_string_chunk(s, "</dl>", buf, len) < 0) return -1;
        if (wificfg_write_string_chunk(s, http_index_content[2], buf, len) < 0) return -1;
        if (wificfg_write_chunk_end(s) < 0) return -1;
    }
    return 0;
}

static const wificfg_dispatch dispatch_list[] = {
    {"/", HTTP_METHOD_GET, handle_index, false},
    {"/index.html", HTTP_METHOD_GET, handle_index, false},
    {NULL, HTTP_METHOD_ANY, NULL}
};


void wifi_cfg(){
	sdk_wifi_set_sleep_type(WIFI_SLEEP_MODEM);
	wificfg_init(80, dispatch_list);
}
