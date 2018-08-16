/*
 * comm.c
 *
 *  Created on: Jan 3, 2018
 *      Author: Renan Augusto Starke
 */

#include "espressif/esp_common.h"

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

#include <stdint.h>
#include "esp/uart.h"

#include "comm.h"
#include "motors.h"

QueueHandle_t tx_queue;
QueueHandle_t rx_queue;

TaskHandle_t xHandlingUartTask;
TaskHandle_t xHandlingPkgTask;

#define DEBUG

#ifdef DEBUG
#define debug(fmt, ...) printf("%s: " fmt "\n", "PWM", ## __VA_ARGS__)
#else
#define debug(fmt, ...)
#endif

#define TX_EN_PIN 5
#define RS485_USART 0


volatile uint16_t temperature = 0;

/**
  * @brief  Send data to RS485. TXEN PIN is assrted when trasmiting
  * @param	packet_buffer: pointer to byte packge
  * @param  packet_size: packet size
  *
  * @retval None.
  */
static void send_data_rs485(uint8_t *packet_buffer, uint8_t packet_size){
	uint8_t i = 0;

	/* Assert GPIO TX ENABLE PIN */
	GPIO.OUT_SET = BIT(TX_EN_PIN);
	/* Copy data to CPU TX Fifo */
	for (i=0; i < packet_size; i++)
		uart_putc(0, packet_buffer[i]);

	/* Wait data to be sent */
	uart_flush_txfifo(0);
	/* Release RS485 Line */
	GPIO.OUT_CLEAR = BIT(TX_EN_PIN);
}



void  status_task(void *pvParameters)
{
    TickType_t my_time; // = xTaskGetTickCount();
    uint8_t i, error = 0;
    uint8_t rx_pkg[16], pkg[8] = {0x07, 0x1e, 0x83, 0x88, 0xff};

    gpio_enable(TX_EN_PIN, GPIO_OUTPUT);

    //xSemaphoreTake(wifi_alive, portMAX_DELAY);

    while (1) {
       	vTaskDelay(1000 / portTICK_PERIOD_MS);

        send_data_rs485(pkg,5);
        my_time = xTaskGetTickCount();
        error = 0;

		/* Get received data */
		for (i=0; (i < 13);) {
			int data = uart_getc_nowait(0);

			if (data != -1){
				rx_pkg[i] = data;
				i++;
				continue;
			}
			/* Timeout */
			if (xTaskGetTickCount() > (my_time + 50)){
				puts("No RS485 response");
				error = 1;
				break;
			}
		}

		if (!error) {
#ifdef DEBUGG
			for (i=0; i < 13; i++)
				printf("%x-", rx_pkg[i]);
			puts("");
#endif

			temperature = (rx_pkg[3] << 8) | rx_pkg[4];
			printf("%d.%d\n", temperature/10, temperature % 10);
		}
		 //temperature = (s[3] << 8) | s[4]

//        if (xQueueSend(tx_queue, (void *)pkg, 0) == pdFALSE) {
//        	debug("uart_queue overflow.\r\n");
//        }
    }
}


void  uart_task(void *pvParameters){
	BaseType_t xResult;
	uint32_t ulNotifiedValue;

	uint8_t pkg[PKG_MAX_SIZE] = { 0, 0, 0x13, 0, 0};
	uint8_t i;
	uint8_t size;
	uint8_t retries;

	for (;;){
		xResult = xTaskNotifyWait( pdFALSE,          /* Don't clear bits on entry. */
		                           ULONG_MAX,        /* Clear all bits on exit. */
		                           &ulNotifiedValue, /* Stores the notified value. */
								   portMAX_DELAY);

		if (xResult != pdTRUE){
			debug("uart_task: eror on xTaskNotifyWait\r\n");
		}

#ifdef DEBUG_MSGS
		printf("got uart message\r\n");
#endif

		while(xQueueReceive(tx_queue, (void *)pkg, 0) ==
		                  pdTRUE){
		 	/* Send a package */
			makeAndSend(pkg, pkg[1]);
			retries = 30;
			size = 0;

			/* Wait for response: at least 2 bytes (header) */
			while (size < 2 && (retries)){
				vTaskDelay( 200 / portTICK_PERIOD_MS );
				size = uart_rxfifo_size(0);
				retries--;
			}

			/* No response, try to send again */
			if (!retries){
				vTaskDelay( 2000 / portTICK_PERIOD_MS );
				uart_flush_rxfifo(0);
				debug("Uart task: no response\n\r");
				break;
			}

			/* Get received data */
			for (i=0; (i < 2) & (i < PKG_MAX_SIZE); i++)
				pkg[i] = uart_getc_nowait(0);

			/* Check for a valid start byte */
			if (pkg[0] != PKG_START){
				 taskYIELD();
				 //continue;
				 break;
			}

			/* Get remaining data: data and checksum  */
			size = (pkg[1] < PKG_MAX_SIZE) ? (pkg[1] + 1) : (PKG_MAX_SIZE - 1);
			size = uart_rxfifo_wait(0, size);

			for (i=0; i < size; i++)
				pkg[i+2] = uart_getc_nowait(0);

#ifdef DEBUG_MSGS
			for (i=0; i < size + PKG_HEADER_SIZE; i++)
				printf("%x ", pkg[i]);

			printf("\r\n");
#endif

			/* Send to parser task */
			if (xQueueSend(rx_queue, (void *)pkg, 0) == pdFALSE) {
				debug("rx_queue queue overflow.\r\n");
			}
			/* Unblock parser task */
			xTaskNotify( xHandlingPkgTask, 0, eNoAction );
		}
	}
}

void  pkgParser_task(void *pvParameters)
{
	uint8_t pkg[PKG_MAX_SIZE];

    BaseType_t xResult;
    uint32_t ulNotifiedValue;

    uint8_t i;
    uint16_t currentData;

    while (1) {

    	xResult = xTaskNotifyWait( pdFALSE,          /* Don't clear bits on entry. */
								ULONG_MAX,       	 /* Clear all bits on exit. */
							   &ulNotifiedValue, 	 /* Stores the notified value. */
							   portMAX_DELAY);

    	if (xResult != pdTRUE){
    		printf("pkgParser_task: eror on xTaskNotifyWait \r\n");
    	}

#ifdef DEBUG_MSGS
    	printf("pkgParser_task\r\n");
#endif

   	    while(xQueueReceive(rx_queue, (void *)pkg, 0) == pdTRUE){


   	    	switch (pkg[2]) {
				case SERVO_ACK_CMD:
#ifdef DEBUG_MSGS
					printf("PWM ack: %u %u\n", pkg[3], pkg[4]);
#endif
					break;
				case ADC_DATA:

					break;

				case ADC_ACK_CMD:
					for (i=0; i < N_MOTORS*2; i+=2) {
						  currentData = (uint8_t) pkg[3+i] << 8 | (uint8_t)pkg[4+i];
					  	  setCurrent(currentData, i/2);
					}
					break;

				default:
					break;
			}
   	    }
    }
}





void makePkg(uint8_t *data, uint8_t paylodsize){
	uint8_t checksum = 0xff;

	/* Checksum */
	for (uint8_t i = 0; i < paylodsize; i++){
		checksum-= data[i + PKG_HEADER_SIZE];
	}

	data[0] = PKG_START;
	data[1] = paylodsize;
	data[PKG_HEADER_SIZE + paylodsize] = checksum;
}


void makeAndSend(uint8_t *data, uint8_t paylodsize) {
	/* Make pkg */
	makePkg(data, paylodsize);

	/* Send it */
	/* Size: Header + payload + checksum */
	//send_data(data, PKG_HEADER_SIZE + paylodsize + 1);
}
