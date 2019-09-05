/*
 * pressure.c
 *
 *  Created on: Apr 5, 2019
 *      Author: Renan Augusto Starke
 */

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

#include "espressif/esp_common.h"

#include <stdio.h>
#include <string.h>

#include "app_status.h"


void adc_task(void *pvParameters){

	int i = 0;
	uint16_t adc_data[8];
	uint32_t sum = 0;

	memset(adc_data,0,sizeof(adc_data));

	while (1) {
		vTaskDelay(100 / portTICK_PERIOD_MS);

		/* Get ADC data */
		uint16_t adc = sdk_system_adc_read();

		/* 1/8 fir filter */
		sum = sum + adc - adc_data[i];
		adc_data[i++] = adc;
		i = i & 0x7;

		set_adc(sum >> 3);
	}
}


/* Pressure:
 *
 * Pe: 0 .. 6 BAR
 * Out: 4 .. 20 mA
 *
 * P(i) = 375 * i - 1.5
 * 
 * ------------------------
 * Generic:
 * 
 * P(i) = alpha * i + beta
 * 
 * alpha = Full_Scale 
 *         ----------
 *           0.016
 * 
 * beta = - Full_Scale * 0.004       Full_Scale
 *          ------------------  =  - -----------
 *               0.016                    4
 * 
 * i * R_shunt        R_ADC_1
 * ----------- *  ---------------- = VinADC
 *   AmpopG        R_ADC1 + RADC2
 *
 * AmpopG = 2  R_shunt = 680 / 2 = 340  R_ADC1 = 100k R_ADC2 = 220K  		 *
 *
 * R_shunt + AMPOP = 340 || 30k = 336,16
 *
 * VinADC = i * 52.530
 *
 *         VinADC
 * ADC = -------- * 1024
 *          Vref
 *
 *  -----A0     <--- Board connection
 *    |
 *  220K
 *    |--- ADC  <-- ESP Analog input
 *  100K
 *    |
 *   GND
 *
 *            375 * ADC
 * P(ADC) = ------------- - 1.5  (For 0...6 BAR)
 *          1024 * 52530
 *
 *            alpha * ADC
 * P(ADC) = ------------- - beta  (For 0...6 BAR)
 *          1024 * 52530
 * 
 * 
 * 
 * 
 */

void pressure_task(void *pvParameters){

	int p = 0;

	while (1) {
		vTaskDelay(1000 / portTICK_PERIOD_MS);

		int32_t alpha = get_pressure_alpha();
		int32_t beta = get_pressure_beta();

		/* Get ADC filtered data */
		// p = get_adc() * 375000;
		// p = p / 53790 - 1500;
		p = get_adc() * alpha;
		p = p / 53790 - beta;

		/* Convert BAR to kgfm/cm2 : 1 BAR = 1.01972 */
		p = p * 10197;
		p = p / 10000;

		if (p < -500)
			set_pressure_error();
		else
			clear_pressure_error();

		if (p < 0)
			p = 0;

		set_pressure(p);
	}
}
