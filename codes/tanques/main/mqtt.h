/*
 * mqtt.h
 *
 *  Created on: Feb 10, 2020
 *      Author: Renan Augusto Starke
 */

#ifndef MQTT_H_
#define MQTT_H_

#define PUB_MSG_LEN 16
#define PUB_TPC_LEN 64
#define CFG_TOPICS 7

typedef struct publisher_data{
	char topic[PUB_TPC_LEN];
	char data[PUB_MSG_LEN];
} publisher_data_t;

void mqtt_app_start(void);

#endif /* MQTT_H_ */
