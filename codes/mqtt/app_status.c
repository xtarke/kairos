/*
 * app_status.c
 *
 *  Created on: Sep 5, 2018
 *      Author: xtarke
 */

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

struct system_status_t {
	uint16_t temperature;
	uint8_t rs485_addr;
};

volatile struct system_status_t sys_status = { 0, 0x07 };

SemaphoreHandle_t sys_data_mutex;

void app_status_init(){
	vSemaphoreCreateBinary(sys_data_mutex);
}

void set_temperature(uint16_t temp){
  	if (xSemaphoreTake(sys_data_mutex, portMAX_DELAY) == pdTRUE ){
		sys_status.temperature = temp;
		xSemaphoreGive(sys_data_mutex);
  	}
}

uint16_t get_temperature(){
	uint16_t temp = 0;

	if (xSemaphoreTake(sys_data_mutex, portMAX_DELAY) == pdTRUE ){
		temp = sys_status.temperature;
		xSemaphoreGive(sys_data_mutex);
	}

	return temp;
}

void set_rs485_addr(uint8_t addr){
	if (xSemaphoreTake(sys_data_mutex, portMAX_DELAY) == pdTRUE ){
		sys_status.rs485_addr = addr;
		xSemaphoreGive(sys_data_mutex);
	}
}

uint8_t get_rs485_addr(){
	uint8_t addr = 0;

	if (xSemaphoreTake(sys_data_mutex, portMAX_DELAY) == pdTRUE ){
		addr = sys_status.rs485_addr;
		xSemaphoreGive(sys_data_mutex);
	}

	return addr;
}




