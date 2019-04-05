/*
 * app_status.c
 *
 *  Created on: Sep 5, 2018
 *      Author: xtarke
 */

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include "app_status.h"

struct system_status_t {
	int16_t temperature;
	uint8_t rs485_addr;
	uint8_t is_enable;
	uint8_t error;
};

struct system_pressure_t {
	uint16_t adc;
	uint32_t pressure;
};

volatile struct system_status_t sys_status[MAX_485_SENSORS] = { {0, 0x0a, 1,0 },
		{0, 0x0e, 1, 0}, {0, 0x0b, 0, 0}, {0, 0x0c, 0, 0} };

volatile struct system_pressure_t sys_pressure = {0, 0};

SemaphoreHandle_t sys_data_mutex;

void app_status_init(){
	vSemaphoreCreateBinary(sys_data_mutex);
}


void set_adc(uint16_t data){

	if (xSemaphoreTake(sys_data_mutex, portMAX_DELAY) == pdTRUE ){
		sys_pressure.adc = data;
		xSemaphoreGive(sys_data_mutex);
	}
}

uint16_t get_adc(){

	uint16_t data = 0;

	if (xSemaphoreTake(sys_data_mutex, portMAX_DELAY) == pdTRUE ){
		data = sys_pressure.adc;
		xSemaphoreGive(sys_data_mutex);
	}

	return data;
}


void set_temperature(int16_t temp, uint8_t error, uint8_t i){
  	if (xSemaphoreTake(sys_data_mutex, portMAX_DELAY) == pdTRUE ){
  		if (i < MAX_485_SENSORS){
  			sys_status[i].temperature = temp;
  			sys_status[i].error = error;
  		}
		xSemaphoreGive(sys_data_mutex);
  	}
}

void get_temperature(uint8_t i, temperature_stauts_t *t){
	int16_t temp = -100;

	if (xSemaphoreTake(sys_data_mutex, portMAX_DELAY) == pdTRUE ){
		if (i < MAX_485_SENSORS){
			t->temperature = sys_status[i].temperature;
			t->error = sys_status[i].error;
		}
		xSemaphoreGive(sys_data_mutex);
	}
}

void set_rs485_addr(uint8_t i, uint8_t addr){
	if (xSemaphoreTake(sys_data_mutex, portMAX_DELAY) == pdTRUE ){
		if (i < MAX_485_SENSORS)
			sys_status[i].rs485_addr = addr;
		xSemaphoreGive(sys_data_mutex);
	}
}

uint8_t get_rs485_addr(uint8_t i){
	uint8_t addr = 0;

	if (xSemaphoreTake(sys_data_mutex, portMAX_DELAY) == pdTRUE ){
		if (i < MAX_485_SENSORS)
			addr = sys_status[i].rs485_addr;
		xSemaphoreGive(sys_data_mutex);
	}

	return addr;
}

void set_error(uint8_t i, uint8_t error){
	if (xSemaphoreTake(sys_data_mutex, portMAX_DELAY) == pdTRUE ){
		if (i < MAX_485_SENSORS)
			sys_status[i].error = error;
		xSemaphoreGive(sys_data_mutex);
	}
}

uint8_t get_error(uint8_t i){
	uint8_t error = 10;

	if (xSemaphoreTake(sys_data_mutex, portMAX_DELAY) == pdTRUE ){
		if (i < MAX_485_SENSORS)
			error = sys_status[i].error;
		xSemaphoreGive(sys_data_mutex);
	}

	return error;
}

void set_enable(uint8_t i){
	if (xSemaphoreTake(sys_data_mutex, portMAX_DELAY) == pdTRUE ){
		if (i < MAX_485_SENSORS)
			sys_status[i].is_enable = 1;
		xSemaphoreGive(sys_data_mutex);
	}
}

void unset_enable(uint8_t i){
	if (xSemaphoreTake(sys_data_mutex, portMAX_DELAY) == pdTRUE ){
		if (i < MAX_485_SENSORS)
			sys_status[i].is_enable = 0;
		xSemaphoreGive(sys_data_mutex);
	}
}

uint8_t get_enable(uint8_t i){
	uint8_t enable = 0;

	if (xSemaphoreTake(sys_data_mutex, portMAX_DELAY) == pdTRUE ){
		if (i < MAX_485_SENSORS)
			enable = sys_status[i].is_enable;
		xSemaphoreGive(sys_data_mutex);
	}

	return enable;
}



