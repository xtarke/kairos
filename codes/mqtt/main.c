#include "espressif/esp_common.h"
#include "esp/uart.h"

#include <string.h>

#include <FreeRTOS.h>
#include <task.h>
#include <ssid_config.h>

#include <espressif/esp_sta.h>
#include <espressif/esp_wifi.h>

#include <paho_mqtt_c/MQTTESP8266.h>
#include <paho_mqtt_c/MQTTClient.h>

#include <semphr.h>
#include <queue.h>

#include "network.h"
#include "pressure.h"
#include "comm.h"

#include "sysparam.h"
#include "app_status.h"

// #define DEBUG

#ifdef DEBUG
#define debug(fmt, ...) printf("%s: " fmt "\n", "PWM", ## __VA_ARGS__)
#else
#define debug(fmt, ...)
#endif


static void  status_publish_task(void *pvParameters)
{
	int len;
	TickType_t xLastWakeTime = xTaskGetTickCount();

	temperature_stauts_t myData = {0,0};
    publisher_data_t to_publish_data;
    publisher_data_t to_publish_pressure;

    memset(&to_publish_pressure, 0, sizeof(to_publish_pressure));
    memset(&to_publish_data, 0, sizeof(to_publish_data));

    char *wifi_my_host_name = NULL;

	/* Get MQTT host IP from web server configuration */
	sysparam_get_string("hostname", &wifi_my_host_name);

	if (!wifi_my_host_name){
		printf("Invalid host name\n");
		strncpy(to_publish_data.topic,"host_1/temperatura/",PUB_TPC_LEN);
		strncpy(to_publish_pressure.topic,"host_1/pressure/",PUB_TPC_LEN);
	}
	else {
		strncpy(to_publish_data.topic, wifi_my_host_name,PUB_TPC_LEN);
		strncpy(to_publish_pressure.topic, wifi_my_host_name,PUB_TPC_LEN);
		free(wifi_my_host_name);
		strncat(to_publish_data.topic,"/temperatura/0",PUB_TPC_LEN);
		strncat(to_publish_pressure.topic,"/pressure",PUB_TPC_LEN);
	}


    while (1) {
        vTaskDelayUntil(&xLastWakeTime, 10000 / portTICK_PERIOD_MS);

        uint32_t pressure = get_pressure();
        uint8_t error = get_pressure_error();

        snprintf(to_publish_pressure.data, PUB_MSG_LEN, "%d.%d;%d", pressure/1000, pressure%1000, error);
		// snprintf(to_publish_pressure.data, PUB_MSG_LEN, "%d.%d", pressure/1000, pressure%1000);

        if (xQueueSend(publish_queue, (void *)&to_publish_pressure, 0) == pdFALSE) {
        	debug("Publish queue overflow.\r\n");
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

				if (xQueueSend(publish_queue, (void *)&to_publish_data, 0) == pdFALSE) {
					debug("Publish queue overflow.\r\n");
				}
				vTaskDelayUntil(&xLastWakeTime, 1000 / portTICK_PERIOD_MS);
        	}
        }
    }
}

static void  hearbeat_task(void *pvParameters)
{
	GPIO.ENABLE_OUT_SET = BIT(4);
	IOMUX_GPIO4 = IOMUX_GPIO4_FUNC_GPIO | IOMUX_PIN_OUTPUT_ENABLE; /* change this line if you change 'gpio' */

	while(1) {
		/* Disable SoftAP mode if connected */
		if (sdk_wifi_station_get_connect_status() == STATION_GOT_IP){
			sdk_wifi_set_opmode(STATION_MODE);

			GPIO.OUT_SET = BIT(4);
			vTaskDelay(1000 / portTICK_PERIOD_MS);
			GPIO.OUT_CLEAR = BIT(4);
			vTaskDelay(1000 / portTICK_PERIOD_MS);
		}
		else {
			GPIO.OUT_SET = BIT(4);
			vTaskDelay(200 / portTICK_PERIOD_MS);
			GPIO.OUT_CLEAR = BIT(4);
			vTaskDelay(200 / portTICK_PERIOD_MS);
		}

	}
}

void user_init(void)
{
    uart_set_baud(0, 9600);
    uart_set_baud(1, 9600);

    /* Activate UART 1 PIN: NodeMCU's LED is the same GPIO (2 or D4) */
    gpio_set_iomux_function(2, IOMUX_GPIO2_FUNC_UART1_TXD);

    printf("SDK version:%s\n", sdk_system_get_sdk_version());

    publish_queue = xQueueCreate(8, sizeof(publisher_data_t));
    command_queue = xQueueCreate(4, sizeof(command_data_t));

    wifi_cfg();

    app_status_init();
    comm_init();

    /* To Do:
     *
     * Get return of xTaskCreate.
     *
     */
    xTaskCreate(&hearbeat_task, "led_task",  256, NULL, 3, NULL);

    xTaskCreate(&status_task, "tank_status_task", 256, NULL, 4, NULL);

    xTaskCreate(&commands_task, "485_cmd_task", 256, NULL, 4, &xHandling_485_cmd_task);

    xTaskCreate(&adc_task, "adc_task", 256, NULL, 4, NULL);
    xTaskCreate(&pressure_task, "pressure_task", 256, NULL, 4, NULL);

    xTaskCreate(&status_publish_task, "status_publish_task", 512, NULL, 6, NULL);
    xTaskCreate(&mqtt_task, "mqtt_task", 1024, NULL, 7, NULL);
}
