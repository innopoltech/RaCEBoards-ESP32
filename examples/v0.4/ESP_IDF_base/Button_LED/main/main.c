#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "Button_LED";

#define BLINK_GPIO          GPIO_NUM_8
#define BUTTON_GPIO         GPIO_NUM_9

void app_main(void)
{
    //Setup
    ESP_LOGI(TAG, "Configured to blink GPIO LED!");

    //Config button pin
    gpio_reset_pin(BUTTON_GPIO);
    gpio_set_direction(BUTTON_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_GPIO, GPIO_PULLUP_ONLY);

    //Config led pin
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    //Work
    while (1) {
        gpio_set_level(BLINK_GPIO, !gpio_get_level(BUTTON_GPIO));
        vTaskDelay(1);
    }
}
