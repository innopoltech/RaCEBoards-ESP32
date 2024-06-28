#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"

/*
    PERIPHERALS USED:
        * TIMER 0
*/
static const char *TAG = "Buzzer_work";

#define BUZZER_GPIO          GPIO_NUM_4
#define FREQUENCY_BUZZER     1000
#define BUZZER_DELAY         300

ledc_timer_config_t timer_conf;
ledc_channel_config_t channel_conf;


void BuzzerOn(void)
{
    //MAX DUTY (LEDC_TIMER_10_BIT) = 1024 
    //MAX VOLUME = (MAX DUTY)/2 = 512
    ledc_set_duty(channel_conf.speed_mode, channel_conf.channel, 512);
    ledc_update_duty(channel_conf.speed_mode, channel_conf.channel);

    ESP_LOGI(TAG, "Buzzer - ON!");
}

void BuzzerOff(void)
{
    ledc_set_duty(channel_conf.speed_mode, channel_conf.channel, 0);
    ledc_update_duty(channel_conf.speed_mode, channel_conf.channel);

    ESP_LOGI(TAG, "Buzzer - OFF!");
}

void app_main(void)
{
    //Setup
    ESP_LOGI(TAG, "Configured to buzzer!");

    //Config timer
    timer_conf.speed_mode      = LEDC_LOW_SPEED_MODE;
    timer_conf.duty_resolution = LEDC_TIMER_10_BIT;
    timer_conf.timer_num       = LEDC_TIMER_0;
    timer_conf.freq_hz         = (uint32_t)FREQUENCY_BUZZER;
    timer_conf.clk_cfg         = LEDC_USE_APB_CLK;

    ledc_timer_config(&timer_conf);

    //Config channel
    channel_conf.gpio_num      = BUZZER_GPIO;
    channel_conf.speed_mode    = LEDC_LOW_SPEED_MODE;
    channel_conf.channel       = LEDC_CHANNEL_0;
    channel_conf.intr_type     = LEDC_INTR_DISABLE;
    channel_conf.timer_sel     = LEDC_TIMER_0;
    channel_conf.duty          = 0;
    channel_conf.hpoint        = 0;

    ledc_channel_config(&channel_conf);

    //Work
    while (1) {

        BuzzerOn();
        vTaskDelay(BUZZER_DELAY / portTICK_PERIOD_MS);
        BuzzerOff();
        vTaskDelay(BUZZER_DELAY / portTICK_PERIOD_MS);
    }
}
