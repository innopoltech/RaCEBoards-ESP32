#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "m_qmc5883l.h"
#include "m_i2c_thread.h"
#include "esp_timer.h"

#include <string.h>

static const char *TAG = "QMC5883L_work";

#define CONFIG_QMC5883L_PERIOD 500

qmc5883l_t qmc5883l_dev;

void app_main(void)
{
    //Setup
    ESP_LOGI(TAG, "Configured to QMC5883L!");

    //Create i2c
    i2c_t_create(); 

    //Configure parameters
    memset(&qmc5883l_dev, 0, sizeof(qmc5883l_t));

    qmc5883l_dev.addr = QMC5883L_I2C_ADDR_DEF;
    qmc5883l_dev.id   = QMC5883L_I2C_ID_DEF;

    qmc5883l_init(&qmc5883l_dev, QMC5883L_DR_50, QMC5883L_OSR_128, QMC5883L_RNG_2);

    float mag_data[3] = {0,0,0}; //x,y,z, mG
    float temp = 0;              // `C

    //Work
    while (1) {
        vTaskDelay(CONFIG_QMC5883L_PERIOD / portTICK_PERIOD_MS);

        if (qmc5883l_get_data(&qmc5883l_dev, mag_data) != ESP_OK)
        {
            ESP_LOGE(TAG, "Magnetic fields reading failed!");
            continue;
        }
        ESP_LOGI(TAG, "Magnetometer: X:%.3f, Y:%.3f, Z:%.3f mG", mag_data[0], mag_data[1], mag_data[2]);

        if (qmc5883l_get_temp(&qmc5883l_dev, &temp) != ESP_OK)
        {
            ESP_LOGE(TAG, "Temp reading failed!");
            continue;
        }
        ESP_LOGI(TAG, "Temperature   : %0.2f C", temp);
    }
}

