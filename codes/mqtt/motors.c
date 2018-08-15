/*
 * motors.c
 *
 *  Created on: Jan 11, 2018
 *      Author: Renan Augusto Starke
 */

#include <stdint.h>
#include <string.h>
#include "motors.h"

/* Servo motor status struct */
typedef struct Motors {
	uint8_t pos[N_MOTORS];
	uint16_t current[N_MOTORS];
} motors_t;

motors_t servosStatus;


void motorsInit(){
	memset(&servosStatus, 0, sizeof(motors_t));
}

void setPos(uint8_t pos, uint8_t i){
	if (i < N_MOTORS) {
		servosStatus.pos[i] = pos;
	}
}

void setCurrent(uint16_t current, uint8_t i){
	if (i < N_MOTORS)
		servosStatus.current[i] = current;
}

uint16_t getCurrent(uint8_t i){

	uint16_t data = 0;

	if (i < N_MOTORS)
		data = servosStatus.current[i];

	return data;
}
