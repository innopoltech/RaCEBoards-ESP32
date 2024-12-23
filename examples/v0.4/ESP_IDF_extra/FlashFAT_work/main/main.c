#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "m_flash_fat.h"
#include "esp_log.h"


static const char *TAG = "FLashFAT";

void app_main(void)
{
    ESP_LOGI(TAG, "Configured to spi FlashFAT!");

    flash_fat_pins_t flash_fat_pins = 
    {
        .MOSI = GPIO_NUM_11,
        .MISO = GPIO_NUM_13,
        .SCK = GPIO_NUM_12,
        .nCS = GPIO_NUM_14,
    };
    flash_fat_mount(&flash_fat_pins, PRO_CPU_NUM);

    flash_fat_test_speed();

    flash_fat_unmount();

}
