/*
 * pressure.c
 *
 *  Created on: Apr 5, 2019
 *      Author: xtarke
 */

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

#include "espressif/esp_common.h"

#include <stdio.h>
#include <string.h>

#include "app_status.h"


void pressure_task(void *pvParameters){

	int i = 0;
	uint16_t adc_data[8];
	uint32_t sum = 0;

	memset(adc_data,0,sizeof(adc_data));

	while (1) {
		vTaskDelay(100 / portTICK_PERIOD_MS);

		/* Get ADC data */
		uint16_t adc = sdk_system_adc_read();

		/* for (int j=0; j < 8; j++)
			printf("%d + ", adc_data[j]); */

		sum = sum + adc - adc_data[i];

		adc_data[i++] = adc;
		i = i & 0x7;

		set_adc(sum >> 3);

        /* printf("%d  %d\n", sum, i); */

	}

}
