#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "m_spi_thread.h"
#include "m_sd_card.h"
#include "esp_log.h"

/*
    PERIPHERALS USED:
        * SDCARD DMA_AUTO
*/
#define RADIO_CS_PIN 7 

static const char *TAG = "SD_file_speed";

void app_main(void)
{
    ESP_LOGI(TAG, "Configured to spi SD_CARD!");

    //CS-HIGH for radio
    gpio_config_t cfg = {
        .pin_bit_mask = BIT64(RADIO_CS_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = false,
        .pull_down_en = false,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    gpio_set_level(RADIO_CS_PIN, 1);

    //Create SPI
    spi_t_create();

    //Mount
    sdcard_mount(spi_t_get_host());

    //Test speed
    sdcard_test_speed();

    //Write once data
    ESP_LOGI(TAG, "Write log once...");
    WriteOnce("The data is recorded by opening and closing the file\n");

    //Write сontinuous data
    ESP_LOGI(TAG, "Write log continuous...");
    WriteContinuous("The data ", true, false);
    WriteContinuous("is recorded ", true, false);
    WriteContinuous("continuous ", true, false);
    WriteContinuous("....\n", true, true);
    WriteContinuous(NULL, false, false);

    //Unmount
    sdcard_unmount();

    ESP_LOGI(TAG, "All done!");
}
