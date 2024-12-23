#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"

#pragma once

/************************ BUZZER ************************/

#define BUZZER_GPIO          GPIO_NUM_4
#define FREQUENCY_BUZZER     1000

void BuzzerInit(void);
void BuzzerOn(void);
void BuzzerOff(void);

/************************ LED ************************/

#define LED_COLOR_NUMBER 4
typedef enum
{
    led_white = 0,
    led_green,
    led_red,
    led_blue,
    led_yellow,
    led_purple,
    led_lightblue,
    led_off,
}led_color_t;

void LedInit(void);
void LedSetColor(led_color_t color);

/************************ GPIO ************************/
#define Button GPIO_NUM_9

#define LED_R GPIO_NUM_40
#define LED_G GPIO_NUM_48
#define LED_B GPIO_NUM_47

void AllGPIOInit(void);