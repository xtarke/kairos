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

#include "motors.h"
#include "network.h"
#include "comm.h"

// #define DEBUG

#ifdef DEBUG
#define debug(fmt, ...) printf("%s: " fmt "\n", "PWM", ## __VA_ARGS__)
#else
#define debug(fmt, ...)
#endif

extern uint16_t temperature;


static void  beat_task(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    char msg[PUB_MSG_LEN];
    int count = 0;

    while (1) {
        vTaskDelayUntil(&xLastWakeTime, 10000 / portTICK_PERIOD_MS);
        debug("beat\r\n");
        snprintf(msg, PUB_MSG_LEN, "%d.%d", temperature/10, temperature%10);
        if (xQueueSend(publish_queue, (void *)msg, 0) == pdFALSE) {
        	debug("Publish queue overflow.\r\n");
        }
    }
}

static void  hearbeat_task(void *pvParameters)
{
	GPIO.ENABLE_OUT_SET = BIT(2);
	IOMUX_GPIO2 = IOMUX_GPIO2_FUNC_GPIO | IOMUX_PIN_OUTPUT_ENABLE; /* change this line if you change 'gpio' */

	while(1) {
		GPIO.OUT_SET = BIT(2);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
		GPIO.OUT_CLEAR = BIT(2);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}


#define AP_SSID "esp-open-rtosAP"
#define AP_PSK "12345678"

void soft_ap_config(){
	sdk_wifi_set_opmode(SOFTAP_MODE);
	struct ip_info ap_ip;
	IP4_ADDR(&ap_ip.ip, 10, 0, 0, 1);
	IP4_ADDR(&ap_ip.gw, 0, 0, 0, 0);
	IP4_ADDR(&ap_ip.netmask, 255, 255, 255, 0);
	sdk_wifi_set_ip_info(1, &ap_ip);

	struct sdk_softap_config ap_config = { .ssid = AP_SSID, .ssid_hidden = 0, .channel = 3, .ssid_len = strlen(AP_SSID), .authmode =
			AUTH_WPA_WPA2_PSK, .password = AP_PSK, .max_connection = 3, .beacon_interval = 100, };
	sdk_wifi_softap_set_config(&ap_config);

	xSemaphoreGive( wifi_alive );
}



void user_init(void)
{
    uart_set_baud(0, 9600);
    uart_set_baud(1, 9600);

    /* Activate UART 1 PIN: NodeMCU's LED is the same GPIO (2 or D4) */
    gpio_set_iomux_function(2, IOMUX_GPIO2_FUNC_UART1_TXD);

    printf("SDK version:%s\n", sdk_system_get_sdk_version());

    vSemaphoreCreateBinary(wifi_alive);
    publish_queue = xQueueCreate(3, PUB_MSG_LEN);

    soft_ap_config();

    /* Serial comm queues */
    tx_queue = xQueueCreate(4, PKG_MAX_SIZE);
    rx_queue = xQueueCreate(4, PKG_MAX_SIZE);


   // xTaskCreate(&wifi_task, "wifi_task",  256, NULL, 2, NULL);
    //xTaskCreate(&hearbeat_task, "led_task",  128, NULL, 3, NULL);

    //xTaskCreate(&uart_task, "uart_task", 256, NULL, 3, &xHandlingUartTask);

    xTaskCreate(&status_task, "status_task", 256, NULL, 4, NULL);
    //xTaskCreate(&pkgParser_task, "pkgParser_task", 256, NULL, 4, &xHandlingPkgTask);

    xTaskCreate(&beat_task, "beat_task", 256, NULL, 6, NULL);
    xTaskCreate(&mqtt_task, "mqtt_task", 1024, NULL, 7, NULL);
}
