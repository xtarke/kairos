/*
 * comm.h
 *
 *  Created on: Jan 3, 2018
 *      Author: Renan Augusto Starke
 */

#ifndef COMM_H_
#define COMM_H_

#include <stdint.h>
#include "esp/uart.h"

typedef struct command_data{
	uint8_t cmd;
	uint16_t data;
} command_data_t;

void status_task(void *pvParameters);
void commands_task(void *pvParameters);

extern TaskHandle_t xHandling_485_cmd_task;

void comm_init();



#endif /* COMM_H_ */
