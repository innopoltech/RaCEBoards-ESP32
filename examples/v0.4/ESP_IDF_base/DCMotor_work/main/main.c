#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"

/*
    PERIPHERALS USED:
        * TIMER 2
*/
static const char *TAG = "DCMotor_work";

/* H-bridge specification
# xIN1 | xIN2 |  FUNCTION
# PWM  | 0    | Forward PWM, fast decay
# 1    | PWM  | Forward PWM, slow decay
# 0    | PWM  | Reverse PWM, fast decay
# PWM  | 1    | Reverse PWM, slow decay
*/
#define DC_SPEED_MIN          -100
#define DC_SPEED_MAX          100

#define DC_PWM_1_GPIO         GPIO_NUM_21
#define DC_BOOL_2_GPIO        GPIO_NUM_47
#define FREQUENCY_DC          50
#define DC_DELAY              50

ledc_timer_config_t timer_conf;
ledc_channel_config_t channel_conf;

int16_t speed_clamp_(int16_t speed)
{
    if(speed < DC_SPEED_MIN)
        return  (int16_t)DC_SPEED_MIN;
    if(speed > DC_SPEED_MAX)
        return  (int16_t)DC_SPEED_MAX;

    return speed;
}

void DCMotorSetSpeed(int16_t speed)
{
    speed = speed_clamp_(speed);
    
    //"Clear" duty cycle
    uint32_t duty = (uint32_t)((float)abs(speed) * 1024.0/100.0);

    //Inversion if reverse 
    duty = speed < 0 ? (1024-duty) : duty;
    uint8_t pin_state = speed < 0 ? 1 : 0;

    //Change parameters
    gpio_set_level(DC_BOOL_2_GPIO, pin_state);

    ledc_set_duty(channel_conf.speed_mode, channel_conf.channel, duty);
    ledc_update_duty(channel_conf.speed_mode, channel_conf.channel);
}

void app_main(void)
{
    //Setup
    ESP_LOGI(TAG, "Configured to DC motor!");

    //Config timer
    timer_conf.speed_mode      = LEDC_LOW_SPEED_MODE;
    timer_conf.duty_resolution = LEDC_TIMER_10_BIT;
    timer_conf.timer_num       = LEDC_TIMER_2;
    timer_conf.freq_hz         = (uint16_t)FREQUENCY_DC;
    timer_conf.clk_cfg         = LEDC_AUTO_CLK;

    ledc_timer_config(&timer_conf);

    //Config channel 0
    channel_conf.gpio_num      = DC_PWM_1_GPIO;
    channel_conf.speed_mode    = LEDC_LOW_SPEED_MODE;
    channel_conf.channel       = LEDC_CHANNEL_0;
    channel_conf.intr_type     = LEDC_INTR_DISABLE;
    channel_conf.timer_sel     = LEDC_TIMER_2;
    channel_conf.duty          = 0;
    channel_conf.hpoint        = 0;

    ledc_channel_config(&channel_conf);

    //Config pin 2
    gpio_reset_pin(DC_BOOL_2_GPIO);
    gpio_set_direction(DC_BOOL_2_GPIO, GPIO_MODE_OUTPUT);

    int16_t speed = 0;
    int8_t dir    = 1;

    //Work
    while (1) {

        //Change direction
        if(dir == 1  && speed == 100)
            dir = -1;
        if(dir == -1 && speed == -100)
            dir = 1;

        //Change speed
        speed += dir;
        DCMotorSetSpeed(speed);


        ESP_LOGI(TAG, "Direction : %d, Speed: %d", dir, speed);
        vTaskDelay(DC_DELAY / portTICK_PERIOD_MS);
    }
}
