/*
 * pressure.c
 *
 *  Created on: Apr 5, 2019
 *      Author: Renan Augusto Starke
 */

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

#include "driver/adc.h"
#include "esp_log.h"

#include <stdio.h>
#include <string.h>

#include "app_status.h"

//#define DEBUG


#ifdef DEBUG
static const char *TAG = "PRESURE";
#endif

uint16_t adc_data[8] = {0};

void config_adc(){
	// 1. init adc
    adc_config_t adc_config;

    // Depend on menuconfig->Component config->PHY->vdd33_const value
    // When measuring system voltage(ADC_READ_VDD_MODE), vdd33_const must be set to 255.
    adc_config.mode = ADC_READ_TOUT_MODE;
    adc_config.clk_div = 8; // ADC sample collection clock = 80MHz/clk_div = 10MHz
    ESP_ERROR_CHECK(adc_init(&adc_config));
}

void adc_task(void *pvParameters){

	int i = 0;	
	uint32_t sum = 0;
	
	memset(adc_data,0,sizeof(adc_data));

	while (1) {
		vTaskDelay(100 / portTICK_PERIOD_MS);

		/* Get ADC data */
		uint16_t adc = 5;
		adc_read(&adc);

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
 * P(i) = 375*i - 1.5   (0..6 BAR) 
 * 
 * i * R_shunt        R_ADC_1
 * ----------- *  ---------------- = VinADC
 *   AmpopG        R_ADC1 + RADC2
 *
 * AmpopG = 2 
 * R_shunt = 680 / 2 = 340  Parallel with 30K (see schematic) = 336.19
 * R_ADC1 = 100k 
 * R_ADC2 = 220K  		
 *
 * R_shunt + AMPOP = 340 || 30k = 336,16
 *
 * VinADC = i * 52.530
 *
 *         VinADC
 * ADC = -------- * 1024
 *          Vref
 * 
 * Vref = ~1V
 *
 *  -----A0     <--- Board connection
 *    |
 *  220K
 *    |--- ADC  <-- ESP Analog input
 *  100K
 *    |
 *   GND
 *
 *            375 * ADC                              375 * ADC
 * P(ADC) = ------------- - 1.5  (For 0...6 BAR) ~= -----------  -1.5  
 *            1024 * 52.530                            53791
 *
 *            alpha * ADC
 * P(ADC) = ------------- - beta  (For 0...6 BAR)
 *              53791
 * 
 */

void pressure_task(void *pvParameters){

	int32_t p = 0;

	while (1) {
		vTaskDelay(1000 / portTICK_PERIOD_MS);

		int32_t alpha = get_pressure_alpha();
		int32_t beta = get_pressure_beta();
		int32_t adc = get_adc();

		/* Get ADC filtered data */
		// p = get_adc() * 375000;
		// p = p / 53790 - 1500;
		p = adc * alpha;
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

#ifdef DEBUG
		ESP_LOGI(TAG,"p (%d) = %d", adc, p);		
		ESP_LOGI(TAG,"alpha = %d", alpha);
		ESP_LOGI(TAG,"beta = %d", beta);
#endif

	}
}
