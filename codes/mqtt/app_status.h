/*
 * app_status.h
 *
 *  Created on: Sep 5, 2018
 *      Author: xtarke
 */

#ifndef APP_STATUS_H_
#define APP_STATUS_H_

void app_status_init();

void set_temperature(uint16_t temp);
uint16_t get_temperature();

uint8_t get_rs485_addr();
void set_rs485_addr(uint8_t addr);



#endif /* APP_STATUS_H_ */
