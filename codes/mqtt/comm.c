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
#include <string.h>
#include "esp/uart.h"

#include "comm.h"

#include "sysparam.h"
#include "network.h"
#include "app_status.h"

#define DEBUG

#ifdef DEBUG
#define debug(fmt, ...) printf("%s: " fmt "\n", "PWM", ## __VA_ARGS__)
#else
#define debug(fmt, ...)
#endif

#define TX_EN_PIN 5
#define RS485_USART 0

TaskHandle_t xHandling_485_cmd_task;
SemaphoreHandle_t rs485_data_mutex;


void comm_init(){
	vSemaphoreCreateBinary(rs485_data_mutex);
}


/**
  * @brief  Send data to RS485. TXEN PIN is asserted when transmitting
  * @param	packet_buffer: pointer to byte package
  * @param  packet_size: packet size
  *
  * @retval None.
  */
static void send_data_rs485(uint8_t *packet_buffer, uint8_t packet_size){
	uint8_t i = 0;

	 xSemaphoreTake(rs485_data_mutex, portMAX_DELAY);
	/* Assert GPIO TX ENABLE PIN */
	GPIO.OUT_SET = BIT(TX_EN_PIN);
	/* Copy data to CPU TX Fifo */
	for (i=0; i < packet_size; i++)
		uart_putc(0, packet_buffer[i]);

	/* Wait data to be sent */
	uart_flush_txfifo(0);
	/* Release RS485 Line */
	GPIO.OUT_CLEAR = BIT(TX_EN_PIN);
	xSemaphoreGive(rs485_data_mutex);
}

static uint8_t receive_data_rs485(uint8_t *rx_pkg, uint8_t packet_size){

	uint8_t error = 0;
	TickType_t my_time;


	my_time = xTaskGetTickCount();
	/* Get received data */
	for (int i=0; (i < packet_size);) {
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

	return error;

}


/**
  * @brief  CRC 16 calculation.
  * @param	buf: pointer to byte package.
  * @param  len: packet size.
  *
  * @retval crc16 value with bytes swapped.
  */
uint16_t CRC16_2(uint8_t *buf, int len)
{
	uint16_t crc = 0xFFFF;
	int i;

	for (i = 0; i < len; i++)
	{
		crc ^= (uint16_t)buf[i];          // XOR byte into least sig. byte of crc

		for (int i = 8; i != 0; i--) {    // Loop over each bit
			if ((crc & 0x0001) != 0) {      // If the LSB is set
				crc >>= 1;                    // Shift right and XOR 0xA001
				crc ^= 0xA001;
			}
			else                            // Else LSB is not set
				crc >>= 1;                    // Just shift right
		}
	}
	// Note, this number has low and high bytes swapped, so use it accordingly (or swap bytes)
	return crc;
}

void status_task(void *pvParameters)
{
    TickType_t my_time;
    uint16_t temperature, crc16;
    uint8_t i, error = 0;
    uint8_t rx_pkg[16], pkg[8] = {0x07, 0x1e, 0x83, 0x88, 0xff};

    memset(rx_pkg, 0, sizeof(rx_pkg));

    /* Enable GPIO for 485 transceiver */
    gpio_enable(TX_EN_PIN, GPIO_OUTPUT);

    while (1) {
       	vTaskDelay(1000 / portTICK_PERIOD_MS);

       	/* Update pkg address and CRC */
       	pkg[0] = get_rs485_addr();
       	crc16 = CRC16_2(pkg,2);
       	pkg[2] = crc16 & 0x00ff;
       	pkg[3] = (crc16 >> 8);

       	send_data_rs485(pkg,5);
        my_time = xTaskGetTickCount();
        error = receive_data_rs485(rx_pkg, 13);


		if (!error) {
#ifdef DEBUGG
			for (i=0; i < 13; i++)
				printf("%x-", rx_pkg[i]);
			puts("");
#endif

			temperature = (rx_pkg[3] << 8) | rx_pkg[4];

			printf("%d.%d\n", temperature/10, temperature % 10);
			printf("CRC: %x\n", CRC16_2(pkg,2));

			set_temperature(temperature);
		}
    }
}


void commands_task(void *pvParameters){

	BaseType_t xResult;
	uint32_t ulNotifiedValue;

	command_data_t rx_data;

	while (1){
		xTaskNotifyWait( pdFALSE,          /* Don't clear bits on entry. */
					   ULONG_MAX,        /* Clear all bits on exit. */
					   &ulNotifiedValue, /* Stores the notified value. */
					   portMAX_DELAY);

		debug("Topic Received\n");

		while(xQueueReceive(command_queue, (void *)&rx_data, 0) == pdTRUE){

			printf("%d   %d\n", rx_data.cmd, rx_data.data);


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
