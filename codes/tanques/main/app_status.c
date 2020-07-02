/*
* app_status.c
*
*  Created on: Sep 5, 2018
*      Author: Renan Augusto Starke
*/

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <stdint.h>

#include "esp_log.h"

#include "nvs.h"
#include "nvs_flash.h"

#include "app_status.h"
#include "bits.h"

struct system_status_t
{
	int16_t temperature;
	uint8_t rs485_addr;
	uint8_t is_enable;
	uint8_t error;
};

struct system_pressure_t
{
	uint32_t pressure;
	uint16_t adc;
	uint8_t is_enable;
	uint8_t error;
	int32_t alpha;
	int32_t beta;
};

volatile struct system_status_t sys_status[MAX_485_SENSORS] = {{0, 0x0a, 1, 0},
															   {0, 0x0e, 1, 0},
															   {0, 0x0b, 0, 0},
															   {0, 0x0c, 0, 0}};

volatile struct system_pressure_t sys_pressure = {0, 0, 1, 0, 375000, 1500};

SemaphoreHandle_t sys_data_mutex;
static const char *TAG = "APP";

void app_status_init()
{
	int8_t sensors = 0;
	int32_t alpha = 0;
	int32_t beta = 0;
	vSemaphoreCreateBinary(sys_data_mutex);

	esp_err_t err;
	nvs_handle my_handle;

	/* Open NVS (Non-Volatile Storage) */
	err = nvs_open("storage", NVS_READWRITE, &my_handle);

	if (err != ESP_OK)
	{
		ESP_LOGI(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
	}
	else
	{
		err = nvs_get_i8(my_handle, "sensors", &sensors);

		if (err != ESP_OK) {
			ESP_LOGI(TAG, "New sensor flash table");
			/* Enable bits:
			bit:     4  3   2  1  0
			sensor:  p  t3 t2  t1 t0
			default is: pressure, t0 and t1 : 0x13 */
			nvs_set_i8(my_handle, "sensors", 0x13);
			nvs_get_i8(my_handle, "sensors", &sensors);
		}

		ESP_LOGI(TAG, "sensors: %x", sensors);

		/* Enable/disable temperature sensors */
		for (int i = 0; i < 4; i++)
		{
			if (TST_BIT(sensors, i))
				set_enable(i);
			else
				unset_enable(i);
		}

		if (TST_BIT(sensors, 4))
			set_pressure_enable();
		else
			set_pressure_disable();

		err = nvs_get_i32(my_handle, "alpha", &alpha);

		if (err != ESP_OK) {
			ESP_LOGI(TAG, "New alpha flash table");
			nvs_set_i32(my_handle, "alpha", 375000);
			nvs_get_i32(my_handle, "alpha", &alpha);
		}

		ESP_LOGI(TAG, "alpha: %x", alpha);
		set_pressure_alpha(alpha);

		err = nvs_get_i32(my_handle, "beta", &beta);

		if (err != ESP_OK) {
			nvs_set_i32(my_handle, "beta", 1500);
			nvs_get_i32(my_handle, "beta", &beta);
		}

		ESP_LOGI(TAG, "beta: %x", beta);
		set_pressure_beta(beta);

		nvs_close(my_handle);
	}
}

void set_adc(uint16_t data)
{

	if (xSemaphoreTake(sys_data_mutex, portMAX_DELAY) == pdTRUE)
	{
		sys_pressure.adc = data;
		xSemaphoreGive(sys_data_mutex);
	}
}

uint16_t get_adc()
{

	uint16_t data = 0;

	if (xSemaphoreTake(sys_data_mutex, portMAX_DELAY) == pdTRUE)
	{
		data = sys_pressure.adc;
		xSemaphoreGive(sys_data_mutex);
	}

	return data;
}

void set_pressure(uint32_t data)
{

	if (xSemaphoreTake(sys_data_mutex, portMAX_DELAY) == pdTRUE)
	{
		sys_pressure.pressure = data;
		xSemaphoreGive(sys_data_mutex);
	}
}

uint32_t get_pressure()
{
	uint32_t data = 0;

	if (xSemaphoreTake(sys_data_mutex, portMAX_DELAY) == pdTRUE)
	{
		data = sys_pressure.pressure;
		xSemaphoreGive(sys_data_mutex);
	}

	return data;
}

void set_pressure_error()
{

	if (xSemaphoreTake(sys_data_mutex, portMAX_DELAY) == pdTRUE)
	{
		sys_pressure.error = 1;
		xSemaphoreGive(sys_data_mutex);
	}
}

uint8_t is_pressure_enable()
{
	uint8_t enable = 0;

	if (xSemaphoreTake(sys_data_mutex, portMAX_DELAY) == pdTRUE)
	{
		enable = sys_pressure.is_enable;
		xSemaphoreGive(sys_data_mutex);
	}

	return enable;
}

uint8_t get_pressure_error()
{
	uint8_t data = 0;

	if (xSemaphoreTake(sys_data_mutex, portMAX_DELAY) == pdTRUE)
	{
		data = sys_pressure.error;
		xSemaphoreGive(sys_data_mutex);
	}
	return data;
}

void clear_pressure_error()
{

	if (xSemaphoreTake(sys_data_mutex, portMAX_DELAY) == pdTRUE)
	{
		sys_pressure.error = 0;
		xSemaphoreGive(sys_data_mutex);
	}
}

int32_t get_pressure_alpha()
{
	int32_t data = 0;

	if (xSemaphoreTake(sys_data_mutex, portMAX_DELAY) == pdTRUE)
	{
		data = sys_pressure.alpha;
		xSemaphoreGive(sys_data_mutex);
	}
	return data;
}

int32_t get_pressure_beta()
{
	int32_t data = 0;

	if (xSemaphoreTake(sys_data_mutex, portMAX_DELAY) == pdTRUE)
	{
		data = sys_pressure.beta;
		xSemaphoreGive(sys_data_mutex);
	}
	return data;
}

void set_pressure_alpha(int32_t alpha)
{
	if (xSemaphoreTake(sys_data_mutex, portMAX_DELAY) == pdTRUE)
	{
		sys_pressure.alpha = alpha;
		xSemaphoreGive(sys_data_mutex);
	}
}

void set_pressure_beta(int32_t beta)
{

	if (xSemaphoreTake(sys_data_mutex, portMAX_DELAY) == pdTRUE)
	{
		sys_pressure.beta = beta;
		xSemaphoreGive(sys_data_mutex);
	}
}

void set_temperature(int16_t temp, uint8_t error, uint8_t i)
{
	if (xSemaphoreTake(sys_data_mutex, portMAX_DELAY) == pdTRUE)
	{
		if (i < MAX_485_SENSORS)
		{
			sys_status[i].temperature = temp;
			sys_status[i].error = error;
		}
		xSemaphoreGive(sys_data_mutex);
	}
}

void get_temperature(uint8_t i, temperature_stauts_t *t)
{

	if (xSemaphoreTake(sys_data_mutex, portMAX_DELAY) == pdTRUE)
	{
		if (i < MAX_485_SENSORS)
		{
			t->temperature = sys_status[i].temperature;
			t->error = sys_status[i].error;
		}
		xSemaphoreGive(sys_data_mutex);
	}
}

void set_rs485_addr(uint8_t i, uint8_t addr)
{
	if (xSemaphoreTake(sys_data_mutex, portMAX_DELAY) == pdTRUE)
	{
		if (i < MAX_485_SENSORS)
			sys_status[i].rs485_addr = addr;
		xSemaphoreGive(sys_data_mutex);
	}
}

uint8_t get_rs485_addr(uint8_t i)
{
	uint8_t addr = 0;

	if (xSemaphoreTake(sys_data_mutex, portMAX_DELAY) == pdTRUE)
	{
		if (i < MAX_485_SENSORS)
			addr = sys_status[i].rs485_addr;
		xSemaphoreGive(sys_data_mutex);
	}

	return addr;
}

void set_error(uint8_t i, uint8_t error)
{
	if (xSemaphoreTake(sys_data_mutex, portMAX_DELAY) == pdTRUE)
	{
		if (i < MAX_485_SENSORS)
			sys_status[i].error = error;
		xSemaphoreGive(sys_data_mutex);
	}
}

uint8_t get_error(uint8_t i)
{
	uint8_t error = 10;

	if (xSemaphoreTake(sys_data_mutex, portMAX_DELAY) == pdTRUE)
	{
		if (i < MAX_485_SENSORS)
			error = sys_status[i].error;
		xSemaphoreGive(sys_data_mutex);
	}

	return error;
}

void set_pressure_enable()
{
	if (xSemaphoreTake(sys_data_mutex, portMAX_DELAY) == pdTRUE)
	{
		sys_pressure.is_enable = 1;
		xSemaphoreGive(sys_data_mutex);
	}
}

void set_pressure_disable()
{
	if (xSemaphoreTake(sys_data_mutex, portMAX_DELAY) == pdTRUE)
	{
		sys_pressure.is_enable = 0;
		xSemaphoreGive(sys_data_mutex);
	}
}

void set_enable(uint8_t i)
{
	if (xSemaphoreTake(sys_data_mutex, portMAX_DELAY) == pdTRUE)
	{
		if (i < MAX_485_SENSORS)
			sys_status[i].is_enable = 1;
		xSemaphoreGive(sys_data_mutex);
	}
}

void unset_enable(uint8_t i)
{
	if (xSemaphoreTake(sys_data_mutex, portMAX_DELAY) == pdTRUE)
	{
		if (i < MAX_485_SENSORS)
			sys_status[i].is_enable = 0;
		xSemaphoreGive(sys_data_mutex);
	}
}

uint8_t get_enable(uint8_t i)
{
	uint8_t enable = 0;

	if (xSemaphoreTake(sys_data_mutex, portMAX_DELAY) == pdTRUE)
	{
		if (i < MAX_485_SENSORS)
			enable = sys_status[i].is_enable;
		xSemaphoreGive(sys_data_mutex);
	}

	return enable;
}

uint8_t save_enable(uint8_t i)
{

	esp_err_t err;
	nvs_handle my_handle;
	int8_t sensors = 0;

	if (i > MAX_485_SENSORS + 1)
		return 0;

	/* Open NVS (Non-Volatile Storage) */
	err = nvs_open("storage", NVS_READWRITE, &my_handle);

	if (err != ESP_OK){
		ESP_LOGI(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
		return 0;
	}
	else	{
		err = nvs_get_i8(my_handle, "sensors", &sensors);

		CPL_BIT(sensors, i);
		/* Enable bits:
		bit:     4  3   2  1  0
		sensor:  p  t3 t2  t1 t0
		default is: pressure, t0 and t1 : 0x13 */
		nvs_set_i8(my_handle, "sensors", sensors);

		if (TST_BIT(sensors, i))
			set_enable(i);
		else
			unset_enable(i);

		if (TST_BIT(sensors, 4))
			set_pressure_enable();
		else
			set_pressure_disable();			

		nvs_close(my_handle);
	}

	return 1;
}
