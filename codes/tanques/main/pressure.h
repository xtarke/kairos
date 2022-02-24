/*
 * pressure.h
 *
 *  Created on: Apr 5, 2019
 *      Author: Renan Augusto Starke
 */

#ifndef PRESSURE_H_
#define PRESSURE_H_

void config_adc();
void adc_task(void *pvParameters);
void pressure_task(void *pvParameters);

#endif /* PRESSURE_H_ */
