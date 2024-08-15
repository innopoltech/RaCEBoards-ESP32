#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"

/*
    PERIPHERALS USED:
        * TIMER 1
*/
static const char *TAG = "Servo_work";

/* Servo specification 
# Rotational Range: 180°
# Pulse Cycle: ca. 20 ms
# Pulse Width: 500-2400 µs
*/

/* PWM specification
# Frequency = 50 Hz
# Duty cycle range (10 bit) = 0 ... 1024
# lsb/us = 0.0512
# Duty cycle 0°   = 26
# Duty cycle 180° = 123
*/

#define SERVO_ANGLE_MIN  0.0
#define SERVO_ANGLE_MAX  180.0
#define SERVO_PWM_MIN    26.0
#define SERVO_PWM_MAX    123.0

#define SERVO_0_GPIO         GPIO_NUM_48
#define SERVO_1_GPIO         GPIO_NUM_45
#define FREQUENCY_SERVO      50
#define SERVO_DELAY          1000

ledc_timer_config_t timer_conf;
ledc_channel_config_t channel_conf_0;
ledc_channel_config_t channel_conf_1;

uint16_t angle_clamp_(uint16_t angle)
{
    if(angle < SERVO_ANGLE_MIN)
        return  (uint16_t)SERVO_ANGLE_MIN;
    if(angle > SERVO_ANGLE_MAX)
        return  (uint16_t)SERVO_ANGLE_MAX;

    return angle;
}

uint16_t map_(uint16_t angle)
{
    return (uint16_t)(((float)angle - SERVO_ANGLE_MIN) * (SERVO_PWM_MAX - SERVO_PWM_MIN) / (SERVO_ANGLE_MAX - SERVO_ANGLE_MIN) + SERVO_PWM_MIN);
}

void servo_0_control(uint16_t angle)
{
    angle = angle_clamp_(angle);

    ledc_set_duty(channel_conf_0.speed_mode, channel_conf_0.channel, (uint32_t)map_(angle));
    ledc_update_duty(channel_conf_0.speed_mode, channel_conf_0.channel);

    ESP_LOGI(TAG, "Change servo 0 angle - %d", angle);
}


void servo_1_control(uint16_t angle)
{
    angle = angle_clamp_(angle);

    ledc_set_duty(channel_conf_1.speed_mode, channel_conf_1.channel, (uint32_t)map_(angle));
    ledc_update_duty(channel_conf_1.speed_mode, channel_conf_1.channel);

    ESP_LOGI(TAG, "Change servo 1 angle - %d", angle);
}

void app_main(void)
{
    //Setup
    ESP_LOGI(TAG, "Configured to servo 1 and 2!");

    //Config timer
    timer_conf.speed_mode      = LEDC_LOW_SPEED_MODE;
    timer_conf.duty_resolution = LEDC_TIMER_10_BIT;
    timer_conf.timer_num       = LEDC_TIMER_1;
    timer_conf.freq_hz         = (uint16_t)FREQUENCY_SERVO;
    timer_conf.clk_cfg         = LEDC_AUTO_CLK;

    ledc_timer_config(&timer_conf);

    //Config channel 0
    channel_conf_0.gpio_num      = SERVO_0_GPIO;
    channel_conf_0.speed_mode    = LEDC_LOW_SPEED_MODE;
    channel_conf_0.channel       = LEDC_CHANNEL_0;
    channel_conf_0.intr_type     = LEDC_INTR_DISABLE;
    channel_conf_0.timer_sel     = LEDC_TIMER_1;
    channel_conf_0.duty          = 0;
    channel_conf_0.hpoint        = 0;

    //Config channel 1
    channel_conf_1.gpio_num      = SERVO_1_GPIO;
    channel_conf_1.speed_mode    = LEDC_LOW_SPEED_MODE;
    channel_conf_1.channel       = LEDC_CHANNEL_1;
    channel_conf_1.intr_type     = LEDC_INTR_DISABLE;
    channel_conf_1.timer_sel     = LEDC_TIMER_1;
    channel_conf_1.duty          = 0;
    channel_conf_1.hpoint        = 0;

    ledc_channel_config(&channel_conf_0);
    ledc_channel_config(&channel_conf_1);

    //Work
    while (1) {
        servo_0_control(0);
        servo_1_control(180);
        vTaskDelay(SERVO_DELAY / portTICK_PERIOD_MS);
        servo_0_control(180);
        servo_1_control(0);
        vTaskDelay(SERVO_DELAY / portTICK_PERIOD_MS);
    }
}
