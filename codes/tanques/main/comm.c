/*
 * comm.c
 *
 *  Created on: Feb 10, 2020
 *      Author: Renan Augusto Starke
 */

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

#include <stdint.h>
#include <string.h>

#include "driver/uart.h"
#include "driver/gpio.h"

#include "comm.h"
#include "app_status.h"

#ifdef DEBUG
#define debug(fmt, ...) printf(fmt, ## __VA_ARGS__)
#else
#define debug(fmt, ...)
#endif

static const char *TAG = "RS485";

#define RS485_USART (0)
#define BUF_SIZE (128)

TaskHandle_t xHandling_485_cmd_task;
SemaphoreHandle_t rs485_data_mutex;

void comm_init(){

 	// Configure parameters of an UART driver,
    // communication pins and install the driver
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM_0, &uart_config);
    uart_driver_install(UART_NUM_0, BUF_SIZE * 2, 0, 0, NULL, 0);

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

	/* Assert GPIO TX ENABLE PIN */
	//GPIO.OUT_SET = BIT(TX_EN_PIN);
	gpio_set_level(TX_EN_PIN, 1);
	
	/* Copy data to CPU TX Fifo */
	//for (i=0; i < packet_size; i++)
	//	uart_putc(0, packet_buffer[i]);
	uart_write_bytes(UART_NUM_0, (const char *) packet_buffer, packet_size);

	/* Wait data to be sent */
	//uart_flush_txfifo(0);
	uart_wait_tx_done(UART_NUM_0, 0);

	/* Release RS485 Line */
	//GPIO.OUT_CLEAR = BIT(TX_EN_PIN);
	gpio_set_level(TX_EN_PIN, 0);

}
/**
  * @brief Receive data from RS 485
  * @param rx_pkg: pointer to write received data
  * @param packet_size: expected packet size
  * @param timeout: ticks to retry
  *
  *
  * @retval uint8_t: 0 if OK, 1 if error
  */
static uint8_t receive_data_rs485(uint8_t *rx_pkg, uint8_t packet_size, uint16_t timeout){
	uint8_t len = 0;	
	len = uart_read_bytes(UART_NUM_0, rx_pkg, packet_size, timeout);
	return !len;
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
    int16_t temperature = 0, crc16;
    uint8_t retries = 0, error = 0;
    uint8_t rx_pkg[16], pkg[8] = {0x07, 0x1e, 0x83, 0x88, 0xff};

    memset(rx_pkg, 0, sizeof(rx_pkg));

    while (1) {
       	vTaskDelay(1000 / portTICK_PERIOD_MS);
       	
	for (int i=0; i < MAX_485_SENSORS;){            
            error = 0;
    		vTaskDelay(100 / portTICK_PERIOD_MS);
       		if (get_enable(i)) {

				/* Update pkg address and CRC */
				pkg[0] = get_rs485_addr(i);
				crc16 = CRC16_2(pkg,2);
				pkg[2] = crc16 & 0x00ff;
				pkg[3] = (crc16 >> 8);

				if( xSemaphoreTake(rs485_data_mutex, portMAX_DELAY) == pdTRUE ) {
					send_data_rs485(pkg,4);	
					error = receive_data_rs485(rx_pkg, 13, 50);
					xSemaphoreGive(rs485_data_mutex);
				}
				else
					error = 2;
                
                /* Retries up to 5 times with 485 errors */
                if (error == 1){
					retries++;
                    
					ESP_LOGI(TAG,"Error: %d %d", i, retries);
					
					if (retries < 5) continue;
					else{
						/* Set error and try next sensor */
						set_error(i,error);
						retries = 0;
						i++;
						continue;
					}
                }
				/* Check CRC from received package */
				crc16 = CRC16_2(rx_pkg,11);
				if (rx_pkg[12] != (0xff & (crc16 >> 8)) || rx_pkg[11] != (crc16 & 0xff))
					error = 3;

				if (!error)
					temperature = (rx_pkg[3] << 8) | rx_pkg[4];			
				
				/* Atomic set */
				set_temperature(temperature, error, i);
				vTaskDelay(200 / portTICK_PERIOD_MS);
       		}       	
			i++;
		}
       	
    }
}
