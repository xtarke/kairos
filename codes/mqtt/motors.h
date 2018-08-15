/*
 * motors.h
 *
 *  Created on: Jan 11, 2018
 *      Author: Renan Augusto Starke
 */

#ifndef MOTORS_H_
#define MOTORS_H_

#define N_MOTORS 7

void motorsInit();
void setPos(uint8_t pos, uint8_t i);
void setCurrent(uint16_t current, uint8_t i);

uint16_t getCurrent(uint8_t i);

#endif /* MOTORS_H_ */
