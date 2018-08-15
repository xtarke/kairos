/*
 * network.h
 *
 *  Created on: May 2, 2018
 *      Author: xtarke
 */

#ifndef NETWORK_H_
#define NETWORK_H_

#include <semphr.h>

#define PUB_MSG_LEN 16

extern SemaphoreHandle_t wifi_alive;
extern QueueHandle_t publish_queue;

void  wifi_task(void *pvParameters);
void  mqtt_task(void *pvParameters);


#endif /* NETWORK_H_ */
