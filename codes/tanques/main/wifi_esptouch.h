/*
 * wifi_static.h
 *
 *  Created on: Oct 10, 2019
 *      Author: Renan Augusto Starke
 */

#ifndef WIFI_STATIC_H_
#define WIFI_STATIC_H_

/* FreeRTOS event group to signal when we are connected & ready to make a request */
extern EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
extern int CONNECTED_BIT;

void configure_gpios(void);
void initialise_wifi_touch(void);

#endif /* WIFI_STATIC_H_ */