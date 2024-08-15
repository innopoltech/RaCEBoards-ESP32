#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "m_lsm6dsl.h"
#include "m_i2c_thread.h"
#include "esp_timer.h"

#include <string.h>

static const char *TAG = "LSM6DSL_work";

#define CONFIG_LSM6DSL_PERIOD 100

lsm6dsl_t lsm6dsl_dev;

void app_main(void)
{
    //Setup
    ESP_LOGI(TAG, "Configured to LSM6DSL!");

    //Create i2c
    i2c_t_create(); 

    //Configure parameters
    memset(&lsm6dsl_dev, 0, sizeof(lsm6dsl_t));

    lsm6dsl_dev.addr = LSM6DSL_DEFAULT_ADDRESS;
    lsm6dsl_dev.id   = LSM6DSL_CHIP_ID2; //or LSM6DSL_CHIP_ID
    lsm6dsl_dev.rate = LSM6DSL_RATE_1_66K_HZ;
    lsm6dsl_dev.gyro_range = LSM6DSL_RANGE_2000_DPS;
    lsm6dsl_dev.acc_range  = LSM6DSL_RANGE_16G;

    lsm6dsl_init(&lsm6dsl_dev);

    float acc_data[3] = {0,0,0}; //x,y,z, m/s^2
    float gyro_data[3] = {0,0,0}; //x,y,z, dps
    float temp = 0;              // `C

    //Work
    while (1) {
        vTaskDelay(CONFIG_LSM6DSL_PERIOD / portTICK_PERIOD_MS);

        if (lsm6dsl_acceleration(&lsm6dsl_dev, acc_data) != ESP_OK)
        {
            ESP_LOGE(TAG, "Acc reading failed!");
            continue;
        }
        if (lsm6dsl_gyro(&lsm6dsl_dev, gyro_data) != ESP_OK)
        {
            ESP_LOGE(TAG, "Acc reading failed!");
            continue;
        }
        if (lsm6dsl_temperature(&lsm6dsl_dev, &temp) != ESP_OK)
        {
            ESP_LOGE(TAG, "Temp reading failed!");
            continue;
        }
        ESP_LOGI(TAG, "Acc: X:%03.03f, Y:%03.03f, Z:%03.03f m/s^2, Gyro: X:%04.03f, Y:%04.03f, Z:%04.03f dps, temp: %02.02f C", acc_data[0], acc_data[1], acc_data[2] ,
                                                                                                            gyro_data[0], gyro_data[1], gyro_data[2] ,
                                                                                                            temp);
    }
}
