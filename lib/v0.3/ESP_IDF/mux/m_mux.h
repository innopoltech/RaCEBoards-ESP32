#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

#pragma once

#define MUX_S0_GPIO             GPIO_NUM_46
#define MUX_S1_GPIO             GPIO_NUM_0
#define MUX_S2_GPIO             GPIO_NUM_39
#define MUX_INOUT_GPIO          GPIO_NUM_1
#define MUX_INOUT_ADC_CHANNEL   ADC_CHANNEL_0

#define MUX_ENABLE_INFO         true
#define MUX_ENABLE_ERROR        true

void adc_init();
void init_mux();
void mux_select_line(uint8_t line);

void mux_to_in();
void mux_to_out();
void mux_to_analog();
void mux_from_analog();

uint8_t mux_get_digital();
void mux_set_digital(uint8_t val);
int mux_get_analog();
