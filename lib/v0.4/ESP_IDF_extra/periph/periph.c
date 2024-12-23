#include "periph.h"
/************************ BUZZER ************************/

static ledc_timer_config_t timer_conf;
static ledc_channel_config_t channel_conf;
 
void BuzzerOn(void)
{
    //MAX DUTY (LEDC_TIMER_10_BIT) = 1024 
    //MAX VOLUME = (MAX DUTY)/2 = 512
    ledc_set_duty(channel_conf.speed_mode, channel_conf.channel, 512);
    ledc_update_duty(channel_conf.speed_mode, channel_conf.channel);
}

void BuzzerOff(void)
{
    ledc_set_duty(channel_conf.speed_mode, channel_conf.channel, 0);
    ledc_update_duty(channel_conf.speed_mode, channel_conf.channel);
}

void BuzzerInit(void)
{
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
}

/************************ LED ************************/

char led_color_names[LED_COLOR_NUMBER][32] = 
{
    "white",
    "green",
    "red",
    "blue",
    "yellow",
    "purple",
    "lightblue",
    "off"
};

void LedSetColor(led_color_t color)
{
    ESP_LOGI("LED", "Set color - '%s'", led_color_names[color]);

    //Set color
    switch (color)
    {
        case led_white:
        {
            gpio_set_level(LED_R, 1); gpio_set_level(LED_G, 1); gpio_set_level(LED_B, 1);
        }break;

        case led_green:
        {
            gpio_set_level(LED_R, 0); gpio_set_level(LED_G, 1); gpio_set_level(LED_B, 0);
        }break;

        case led_red:
        {
            gpio_set_level(LED_R, 1); gpio_set_level(LED_G, 0); gpio_set_level(LED_B, 0);
        }break;

        case led_blue:
        {
            gpio_set_level(LED_R, 0); gpio_set_level(LED_G, 0); gpio_set_level(LED_B, 1);
        }break;

        case led_yellow:
        {
            gpio_set_level(LED_R, 1); gpio_set_level(LED_G, 1); gpio_set_level(LED_B, 0);
        }break;

        case led_purple:
        {
            gpio_set_level(LED_R, 1); gpio_set_level(LED_G, 0); gpio_set_level(LED_B, 1);
        }break;

        case led_lightblue:
        {
            gpio_set_level(LED_R, 0); gpio_set_level(LED_G, 1); gpio_set_level(LED_B, 1);
        }break;

        case led_off:
        {
            gpio_set_level(LED_R, 0); gpio_set_level(LED_G, 0); gpio_set_level(LED_B, 0);
        }break;

        default:
            break;
    }
}

void LedInit(void)
{
    ESP_LOGI("LED", "Init ok");
}
/************************ GPIO ************************/

void AllGPIOInit(void)
{
    gpio_config_t cfg = {0,};

    //Button
    cfg.pin_bit_mask = BIT64(Button);
    cfg.mode = GPIO_MODE_INPUT;
    cfg.pull_up_en = true;
    cfg.pull_down_en = false;
    cfg.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&cfg);

    //LED
    cfg.pin_bit_mask = BIT64(LED_R);
    cfg.mode = GPIO_MODE_OUTPUT;
    cfg.pull_up_en = false;
    cfg.pull_down_en = false;
    cfg.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&cfg);
    gpio_set_level(LED_R, 0);

    cfg.pin_bit_mask = BIT64(LED_G);
    cfg.mode = GPIO_MODE_OUTPUT;
    cfg.pull_up_en = false;
    cfg.pull_down_en = false;
    cfg.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&cfg);
    gpio_set_level(LED_G, 0);

    cfg.pin_bit_mask = BIT64(LED_B);
    cfg.mode = GPIO_MODE_OUTPUT;
    cfg.pull_up_en = false;
    cfg.pull_down_en = false;
    cfg.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&cfg);
    gpio_set_level(LED_B, 0);
}