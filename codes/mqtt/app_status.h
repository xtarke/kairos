/*
 * app_status.h
 *
 *  Created on: Sep 5, 2018
 *      Author: xtarke
 */

#ifndef APP_STATUS_H_
#define APP_STATUS_H_

#define MAX_485_SENSORS 4

typedef struct temperatures {
	int16_t temperature;
	uint8_t error;
} temperature_stauts_t;

void app_status_init();

void set_temperature(int16_t temp, uint8_t error, uint8_t i);
void get_temperature(uint8_t i, temperature_stauts_t *t);

uint8_t get_rs485_addr(uint8_t i);
void set_rs485_addr(uint8_t i, uint8_t addr);

void unset_enable(uint8_t i);
void set_enable(uint8_t i);
uint8_t get_enable(uint8_t i);

void set_error(uint8_t i, uint8_t error);
uint8_t get_error(uint8_t i);

void set_adc(uint16_t data);
uint16_t get_adc();

#endif /* APP_STATUS_H_ */
