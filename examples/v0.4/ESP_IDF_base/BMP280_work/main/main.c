#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "m_bmp280.h"
#include "m_i2c_thread.h"
#include "esp_timer.h"

#include <string.h>

static const char *TAG = "BMP280_work";

#define CONFIG_BMP280_PERIOD 2000

bmp280_params_t bmp280_params;
bmp280_t bmp280_dev;

TaskHandle_t myTaskHandle = NULL;

void app_main(void)
{
    //Setup
    ESP_LOGI(TAG, "Configured to BMP280!");

    //Create i2c
    i2c_t_create(); 

    //Configure parameters
    bmp280_init_default_params(&bmp280_params);

    memset(&bmp280_dev, 0, sizeof(bmp280_t));
    bmp280_dev.addr = BMP280_I2C_ADDRESS_0;
    bmp280_dev.id   = BMP280_CHIP_ID;
    
    bmp280_init(&bmp280_dev, &bmp280_params);

    float pressure, temperature;

    //Work
    while (1) {
        vTaskDelay(CONFIG_BMP280_PERIOD / portTICK_PERIOD_MS);

        if (bmp280_read_float(&bmp280_dev, &temperature, &pressure) != ESP_OK)
        {
            ESP_LOGE(TAG, "Temperature/pressure reading failed!");
            continue;
        }

        ESP_LOGI(TAG, "Pressure: %.2f Pa, Temperature: %.2f C", pressure, temperature);
    }
}

